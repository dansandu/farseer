#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/operation_scheduler.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/ballotin/string.hpp"
#include "dansandu/farseer/internal/windows/accept_system_operation.hpp"
#include "dansandu/farseer/internal/windows/close_user_operation.hpp"
#include "dansandu/farseer/internal/windows/connect_user_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/listen_user_operation.hpp"
#include "dansandu/farseer/internal/windows/receive_system_operation.hpp"
#include "dansandu/farseer/internal/windows/register_message_consumer_user_operation.hpp"
#include "dansandu/farseer/internal/windows/register_request_callback_user_operation.hpp"
#include "dansandu/farseer/internal/windows/send_bytes_system_operation.hpp"
#include "dansandu/farseer/internal/windows/send_bytes_user_operation.hpp"
#include "dansandu/farseer/internal/windows/send_request_user_operation.hpp"
#include "dansandu/journey/logging.hpp"

#include <winternl.h>

using dansandu::ballotin::string::toWideString;
using dansandu::farseer::exception::SocketNotFoundError;
using dansandu::farseer::internal::windows::accept_system_operation::createAcceptSystemOperation;
using dansandu::farseer::internal::windows::close_user_operation::createCloseUserOperation;
using dansandu::farseer::internal::windows::connect_user_operation::createConnectUserOperation;
using dansandu::farseer::internal::windows::error::getErrorMessageFromCode;
using dansandu::farseer::internal::windows::error::getLastErrorCode;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::listen_user_operation::createListenUserOperation;
using dansandu::farseer::internal::windows::operation::IOperation;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::ISystemOperation;
using dansandu::farseer::internal::windows::operation::IUserOperation;
using dansandu::farseer::internal::windows::receive_system_operation::createReceiveSystemOperation;
using dansandu::farseer::internal::windows::register_message_consumer_user_operation::
    createRegisterMessageConsumerUserOperation;
using dansandu::farseer::internal::windows::register_request_callback_user_operation::
    createRegisterRequestCallbackUserOperation;
using dansandu::farseer::internal::windows::send_bytes_system_operation::createSendBytesSystemOperation;
using dansandu::farseer::internal::windows::send_bytes_user_operation::createSendBytesUserOperation;
using dansandu::farseer::internal::windows::send_request_user_operation::createSendRequestUserOperation;
using dansandu::farseer::internal::windows::socket_container::Socket;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::journey::exception::WideException;

namespace dansandu::farseer::internal::windows::operation_scheduler
{

namespace
{

constexpr auto defaultCompletionKey = invalidSocketIdentifier.getUnderlying();
constexpr auto maximumNumberOfEntries = ULONG{1024};

// state_ holds the number of outstanding operations in the low bits and the aborting flag in the top bit.
constexpr auto abortingFlag = std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1);
constexpr auto drainTimeoutMilliseconds = DWORD{100};

HANDLE initializeIoCompletionPort()
{
    const auto handle = HANDLE{INVALID_HANDLE_VALUE};
    const auto existingCompletionPort = HANDLE{nullptr};
    const auto numberOfConcurrentThreads = DWORD{0};

    const auto completionPort =
        ::CreateIoCompletionPort(handle, existingCompletionPort, defaultCompletionKey, numberOfConcurrentThreads);

    if (completionPort != nullptr)
    {
        return completionPort;
    }

    THROW(std::runtime_error, "Failed to create I/O completion port queue: ", getLastErrorMessage());
}

}

OperationScheduler::OperationScheduler()
    : completionPort_{initializeIoCompletionPort()},
      socketIdentifierSequencer_{invalidSocketIdentifier.getUnderlying() + 1u},
      socketContainer_{},
      state_{0uz},
      thread_{&OperationScheduler::consumeOperations, this}
{
}

OperationScheduler::~OperationScheduler() noexcept
{
    scheduleAbortOperation();

    thread_.join();

    ::CloseHandle(completionPort_);
}

HANDLE OperationScheduler::getCompletionPort()
{
    return completionPort_;
}

void OperationScheduler::scheduleUserOperation(std::unique_ptr<IUserOperation>&& userOperation)
{
    const auto name = userOperation->getName();

    const auto socketIdentifier = userOperation->getSocketIdentifier();

    const auto numberOfBytesTransferred = 0;

    if (state_.fetch_add(1) & abortingFlag)
    {
        --state_;

        THROW(std::logic_error, "Failed to schedule user operation because the operation consumer thread is aborting");
    }

    SCOPE_FAILURE([&] { --state_; });

    const auto postResult = ::PostQueuedCompletionStatus(
        completionPort_, numberOfBytesTransferred, defaultCompletionKey, userOperation.get()
    );

    if (!postResult)
    {
        THROW(
            std::runtime_error, "Failed to schedule ", name, " with socket ID ", socketIdentifier, ": ",
            getLastErrorMessage()
        );
    }

    // The completion port now owns the operation. Ownership is regained after calling GetQueuedCompletionStatusEx.
    userOperation.release();

    LOG_DEBUG("Scheduled ", name, " with socket ID ", socketIdentifier);
}

void OperationScheduler::scheduleSystemOperation(std::unique_ptr<ISystemOperation>&& systemOperation)
{
    const auto name = systemOperation->getName();

    const auto socketIdentifier = systemOperation->getSocketIdentifier();

    // System operations are scheduled on the consumer thread, so they are not rejected while aborting. Anything they
    // post is cancelled and freed by drainOperations.
    ++state_;

    SCOPE_FAILURE([&] { --state_; });

    systemOperation->schedule(*this, socketContainer_);

    // The completion port now owns the operation. Ownership is regained after calling GetQueuedCompletionStatusEx.
    systemOperation.release();

    LOG_DEBUG("Scheduled ", name, " with socket ID ", socketIdentifier);
}

SocketIdentifier OperationScheduler::scheduleConnectUserOperation(
    const std::string& ipAddress, const int port,
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();

    scheduleUserOperation(createConnectUserOperation(socketIdentifier, ipAddress, port, std::move(connectionCallback)));

    return socketIdentifier;
}

SocketIdentifier OperationScheduler::scheduleListenUserOperation(
    const std::string& ipAddress, const int port,
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();

    scheduleUserOperation(createListenUserOperation(socketIdentifier, ipAddress, port, std::move(connectionCallback)));

    return socketIdentifier;
}

void OperationScheduler::scheduleAcceptSystemOperation(const SocketIdentifier listeningSocketIdentifier)
{
    const auto pendingAcceptSocketIdentifier = socketIdentifierSequencer_.generate();

    scheduleSystemOperation(createAcceptSystemOperation(listeningSocketIdentifier, pendingAcceptSocketIdentifier));
}

void OperationScheduler::scheduleReceiveSystemOperation(const SocketIdentifier socketIdentifier)
{
    scheduleSystemOperation(createReceiveSystemOperation(socketIdentifier));
}

void OperationScheduler::scheduleRegisterMessageConsumerUserOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<void(std::any&&)>&& messageConsumer
)
{
    scheduleUserOperation(
        createRegisterMessageConsumerUserOperation(socketIdentifier, protocolIdentifier, std::move(messageConsumer))
    );
}

void OperationScheduler::scheduleRegisterRequestCallbackUserOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<std::any(std::any&&)>&& requestConsumer
)
{
    scheduleUserOperation(
        createRegisterRequestCallbackUserOperation(socketIdentifier, protocolIdentifier, std::move(requestConsumer))
    );
}

void OperationScheduler::scheduleSendBytesUserOperation(
    const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes
)
{
    scheduleUserOperation(createSendBytesUserOperation(socketIdentifier, std::move(bytes)));
}

void OperationScheduler::scheduleSendBytesSystemOperation(
    const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes
)
{
    scheduleSystemOperation(createSendBytesSystemOperation(socketIdentifier, std::move(bytes)));
}

void OperationScheduler::scheduleSendRequestUserOperation(
    const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber protocolSequenceNumber,
    std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer
)
{
    scheduleUserOperation(createSendRequestUserOperation(
        socketIdentifier, protocolSequenceNumber, std::move(bytes), std::move(responseConsumer)
    ));
}

void OperationScheduler::scheduleCloseUserOperation(const SocketIdentifier socketIdentifier)
{
    scheduleUserOperation(createCloseUserOperation(socketIdentifier));
}

void OperationScheduler::scheduleAbortOperation()
{
    state_.fetch_or(abortingFlag);

    // The abort packet is counted like any other packet so that every dequeued packet decrements state_.
    ++state_;

    const auto numberOfBytesTransferred = 0;
    const auto overlapped = LPOVERLAPPED{nullptr};
    const auto postResult =
        ::PostQueuedCompletionStatus(completionPort_, numberOfBytesTransferred, defaultCompletionKey, overlapped);

    if (postResult == 0)
    {
        --state_;

        LOG_ERROR("Failed to schedule the abort operation to the queue: ", getLastErrorMessage());
    }
    else
    {
        LOG_DEBUG("Scheduled the abort operation");
    }
}

void OperationScheduler::invokeOperationFailureHook(IOperation& operation, const DWORD errorCode) noexcept
{
    const auto name = operation.getName();

    const auto socketIdentifier = operation.getSocketIdentifier();

    try
    {
        operation.onFailure(*this, socketContainer_, errorCode);
    }
    catch (const WideException& exception)
    {
        LOG_ERROR(
            "Failure hook of ", name, " with socket ID ", socketIdentifier,
            " failed with message: ", exception.getMessage()
        );
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR(
            "Failure hook of ", name, " with socket ID ", socketIdentifier, " failed with message: ", exception.what()
        );
    }
    catch (...)
    {
        LOG_ERROR("Failure hook of ", name, " with socket ID ", socketIdentifier, " failed with unknown exception");
    }
}

void OperationScheduler::handleOperationExecutionFailure(
    IOperation& operation, const std::wstring_view message, const bool quietDiscard
) noexcept
{
    const auto name = operation.getName();

    const auto socketIdentifier = operation.getSocketIdentifier();

    if (!quietDiscard)
    {
        if (message.empty())
        {
            LOG_ERROR(name, " with socket ID ", socketIdentifier, " execution failed");
        }
        else
        {
            LOG_ERROR(name, " with socket ID ", socketIdentifier, " execution failed: ", message);
        }
    }

    const auto errorCode = DWORD{ERROR_SUCCESS};

    invokeOperationFailureHook(operation, errorCode);
}

bool OperationScheduler::handleSuccessfulOperation(IOperation& operation, const DWORD numberOfBytesTransferred) noexcept
{
    LOG_DEBUG("Executing ", operation.getName(), " with socket ID ", operation.getSocketIdentifier());

    try
    {
        return operation.execute(*this, socketContainer_, numberOfBytesTransferred);
    }
    catch (const SocketNotFoundError&)
    {
        const auto message = std::wstring_view{};
        const auto quietDiscard = true;

        handleOperationExecutionFailure(operation, message, quietDiscard);
    }
    catch (const WideException& exception)
    {
        handleOperationExecutionFailure(operation, exception.getMessage());
    }
    catch (const std::exception& exception)
    {
        handleOperationExecutionFailure(operation, toWideString(exception.what()));
    }
    catch (...)
    {
        handleOperationExecutionFailure(operation);
    }

    return true;
}

void OperationScheduler::handleFailedOperation(IOperation& operation, const DWORD errorCode) noexcept
{
    const auto name = operation.getName();
    const auto socketIdentifier = operation.getSocketIdentifier();
    const auto level = operation.getLoggingLevelFromErrorCode(errorCode);
    const auto message = getErrorMessageFromCode(errorCode);

    LOG(level, name, " with socket ID ", socketIdentifier, " failed: ", message);

    invokeOperationFailureHook(operation, errorCode);
}

void OperationScheduler::consumeOperationsWork()
{
    // Both the operation consumption and draining are implemented here because the operations are processed in batches.
    // If the abort operation is in the middle of the batch then all operations that follow must be drained. Operations
    // preceding the abort are consumed as usual.
    auto draining = false;

    OVERLAPPED_ENTRY entries[maximumNumberOfEntries];

    while (true)
    {
        // All operations are drained if the abort flag is set and the operations counter is zero.
        if (draining && state_.load() == abortingFlag)
        {
            LOG_DEBUG("Successfully drained all operations");

            return;
        }

        ULONG numberOfEntries = 0;

        const DWORD timeout = draining ? drainTimeoutMilliseconds : INFINITE;
        const BOOL alert = FALSE;

        const auto dequeueResult = ::GetQueuedCompletionStatusEx(
            completionPort_, entries, maximumNumberOfEntries, &numberOfEntries, timeout, alert
        );

        if (dequeueResult == FALSE)
        {
            const auto errorCode = getLastErrorCode();

            if (draining && errorCode == WAIT_TIMEOUT)
            {
                // On timeout, state_ is checked again because a failed posting might have decremented it or cancelled
                // I/O completions might still be on their way.
                continue;
            }

            if (draining)
            {
                LOG_ERROR("Failed to drain all operations: ", getErrorMessageFromCode(errorCode));

                return;
            }

            LOG_ERROR(
                "Failed to dequeue operations; starting to drain operations: ", getErrorMessageFromCode(errorCode)
            );

            // The operation consumer thread cannot continue, so new user operations must be rejected and the
            // outstanding ones drained, otherwise they would be posted to a queue that is never read.
            draining = true;

            startDraining();

            continue;
        }

        for (ULONG index = 0; index < numberOfEntries; ++index)
        {
            const auto& entry = entries[index];

            auto operation = std::unique_ptr<IOperation>{static_cast<IOperation*>(entry.lpOverlapped)};

            if (draining)
            {
                // Deleting a null pointer (the abort packet) is a no-op.
                operation.reset();

                --state_;

                continue;
            }

            if (entry.lpCompletionKey == defaultCompletionKey && entry.lpOverlapped == nullptr)
            {
                LOG_DEBUG("Received the abort operation; starting to drain operations");

                --state_;

                draining = true;

                startDraining();

                continue;
            }

            const auto rescheduled = consumeOperation(*operation, entry);

            if (rescheduled)
            {
                // The operation was rescheduled and awaits completion. Until then, ownership is lost.
                operation.release();
            }
            else
            {
                const auto name = operation->getName();

                const auto socketIdentifier = operation->getSocketIdentifier();

                operation.reset();

                LOG_DEBUG("Erased ", name, " with socket ID ", socketIdentifier);

                --state_;
            }
        }
    }
}

bool OperationScheduler::consumeOperation(IOperation& operation, const OVERLAPPED_ENTRY& entry) noexcept
{
    // The Internal member is documented as reserved, but it holds the NTSTATUS of the completed I/O. Like
    // GetQueuedCompletionStatus, only negative statuses (errors and warnings) are treated as failures.
    if (const auto status = static_cast<NTSTATUS>(entry.Internal); status < 0)
    {
        handleFailedOperation(operation, RtlNtStatusToDosError(status));

        return false;
    }

    const auto discard = handleSuccessfulOperation(operation, entry.dwNumberOfBytesTransferred);

    return !discard;
}

void OperationScheduler::startDraining() noexcept
{
    // The abort flag might not be set if draining starts because of a dequeue failure.
    state_.fetch_or(abortingFlag);

    // Cancels any pending socket I/O, waking up GetQueuedCompletionStatusEx.
    socketContainer_.eraseAllSockets();
}

void OperationScheduler::consumeOperations()
{
    LOG_DEBUG("Started operations consumer thread");

    try
    {
        consumeOperationsWork();
    }
    catch (const WideException& exception)
    {
        LOG_CRITICAL("Operations consumer thread exited with wide exception: ", exception.getMessage());
    }
    catch (const std::exception& exception)
    {
        LOG_CRITICAL("Operations consumer thread exited with exception: ", exception.what());
    }
    catch (...)
    {
        LOG_CRITICAL("Operations consumer thread exited with unknown exception");
    }

    LOG_DEBUG("Exiting operations consumer thread");
}

}
#endif
