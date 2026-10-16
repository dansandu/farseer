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

using dansandu::ballotin::string::toWideString;
using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::exception::SocketNotFoundError;
using dansandu::farseer::internal::windows::accept_system_operation::createAcceptSystemOperation;
using dansandu::farseer::internal::windows::close_user_operation::createCloseUserOperation;
using dansandu::farseer::internal::windows::connect_user_operation::createConnectUserOperation;
using dansandu::farseer::internal::windows::error::getErrorMessageFromCode;
using dansandu::farseer::internal::windows::error::getLastErrorCode;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::listen_user_operation::createListenUserOperation;
using dansandu::farseer::internal::windows::operation::defaultCompletionKey;
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

    THROW(std::runtime_error, "Couldn't create I/O completion port queue: ", getLastErrorMessage());
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

        THROW(std::logic_error, "Operation scheduler is aborting");
    }

    SCOPE_FAILURE([&] { --state_; });

    const auto postResult = ::PostQueuedCompletionStatus(
        completionPort_, numberOfBytesTransferred, defaultCompletionKey, userOperation.get()
    );

    if (!postResult)
    {
        THROW(
            std::runtime_error, "Scheduling ", name, " with socket ID ", socketIdentifier, " failed with error ",
            getLastErrorMessage()
        );
    }

    // The completion port now owns the operation. Ownership is regained by calling GetQueuedCompletionStatus.
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

    // The completion port now owns the operation. Ownership is regained when calling GetQueuedCompletionStatus.
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

        LOG_ERROR("Couldn't schedule AbortOperation to the queue: ", getLastErrorMessage());
    }
    else
    {
        LOG_DEBUG("Scheduled AbortOperation");
    }
}

void OperationScheduler::invokeOperationFailureHook(IOperation& operation, const DWORD errorCode)
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
)
{
    const auto name = operation.getName();

    const auto socketIdentifier = operation.getSocketIdentifier();

    if (!quietDiscard)
    {
        if (message.empty())
        {
            LOG_ERROR(name, " with socket ID ", socketIdentifier, " failed and the socket will be erased");
        }
        else
        {
            LOG_ERROR(name, " with socket ID ", socketIdentifier, " failed and the socket will be erased: ", message);
        }
    }

    const auto errorCode = DWORD{ERROR_SUCCESS};

    invokeOperationFailureHook(operation, errorCode);
}

bool OperationScheduler::handleSuccessfulOperation(IOperation& operation, const DWORD numberOfBytesTransferred)
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

void OperationScheduler::handleFailedOperation(IOperation& operation, const DWORD errorCode)
{
    const auto name = operation.getName();
    const auto socketIdentifier = operation.getSocketIdentifier();
    const auto level = operation.getLoggingLevelFromErrorCode(errorCode);
    const auto message = getErrorMessageFromCode(errorCode);

    LOG(level, name, " with socket ID ", socketIdentifier, " failed and the socket will be erased: ", message);

    invokeOperationFailureHook(operation, errorCode);
}

void OperationScheduler::drainOperations()
{
    LOG_DEBUG("Draining all operations");

    state_.fetch_or(abortingFlag);

    // Cancels any pending sockets I/O, waking up GetQueuedCompletionStatus.
    socketContainer_.eraseAllSockets();

    while ((state_.load() & ~abortingFlag) > 0)
    {
        auto numberOfBytesTransferred = DWORD{0};
        auto completionKey = 0ULL;
        auto overlapped = LPOVERLAPPED{nullptr};

        const auto dequeueResult = ::GetQueuedCompletionStatus(
            completionPort_, &numberOfBytesTransferred, &completionKey, &overlapped, drainTimeoutMilliseconds
        );

        if (dequeueResult == TRUE || overlapped != nullptr)
        {
            // Deleting a null pointer (the abort packet) is a no-op.
            delete static_cast<IOperation*>(overlapped);

            --state_;
        }
        else if (const auto errorCode = getLastErrorCode(); errorCode != WAIT_TIMEOUT)
        {
            LOG_ERROR("Failed to drain all operations due to error: ", getErrorMessageFromCode(errorCode));

            return;
        }

        // On timeout, state_ is checked again because a failed posting might have decremented it or cancelled I/O
        // completions might still be on their way.
    }

    LOG_DEBUG("Successfully drained all operations");
}

void OperationScheduler::consumeOperationsWork()
{
    while (true)
    {
        auto numberOfBytesTransferred = DWORD{0};
        auto completionKey = 0ULL;
        auto overlapped = LPOVERLAPPED{nullptr};
        auto timeout = INFINITE;

        const auto dequeueResult = ::GetQueuedCompletionStatus(
            completionPort_, &numberOfBytesTransferred, &completionKey, &overlapped, timeout
        );

        auto operation = std::unique_ptr<IOperation>{static_cast<IOperation*>(overlapped)};

        if (dequeueResult == TRUE)
        {
            if (completionKey == defaultCompletionKey && overlapped == nullptr)
            {
                LOG_DEBUG("Received abort operation");

                --state_;

                return;
            }

            const auto name = operation->getName();

            const auto socketIdentifier = operation->getSocketIdentifier();

            const auto discard = handleSuccessfulOperation(*operation, numberOfBytesTransferred);

            if (discard)
            {
                operation.reset();

                --state_;

                LOG_DEBUG("Erased ", name, " with socket ID ", socketIdentifier);
            }
            else
            {
                operation.release();
            }
        }
        else
        {
            const auto errorCode = getLastErrorCode();

            if (overlapped == nullptr)
            {
                const auto errorMessage = getErrorMessageFromCode(errorCode);

                WTHROW(InternalSocketError, "Could not dequeue operation from completion queue: ", errorMessage);
            }

            --state_;

            handleFailedOperation(*operation, errorCode);

            operation.reset();
        }
    }
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

    drainOperations();
}

}
#endif
