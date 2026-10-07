#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/operation_scheduler.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/ballotin/string.hpp"
#include "dansandu/farseer/internal/windows/accept_operation.hpp"
#include "dansandu/farseer/internal/windows/close_operation.hpp"
#include "dansandu/farseer/internal/windows/connect_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/listen_operation.hpp"
#include "dansandu/farseer/internal/windows/receive_operation.hpp"
#include "dansandu/farseer/internal/windows/register_message_consumer_operation.hpp"
#include "dansandu/farseer/internal/windows/register_request_callback_operation.hpp"
#include "dansandu/farseer/internal/windows/send_bytes_operation.hpp"
#include "dansandu/farseer/internal/windows/send_request_operation.hpp"
#include "dansandu/journey/logging.hpp"

using dansandu::ballotin::string::toWideString;
using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::windows::accept_operation::createAcceptOperation;
using dansandu::farseer::internal::windows::close_operation::createCloseOperation;
using dansandu::farseer::internal::windows::connect_operation::createConnectOperation;
using dansandu::farseer::internal::windows::error::getErrorMessageFromCode;
using dansandu::farseer::internal::windows::error::getLastErrorCode;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::listen_operation::createListenOperation;
using dansandu::farseer::internal::windows::operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::operation::INonUserOperation;
using dansandu::farseer::internal::windows::operation::IOperation;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::Socket;
using dansandu::farseer::internal::windows::receive_operation::createReceiveOperation;
using dansandu::farseer::internal::windows::register_message_consumer_operation::createRegisterMessageConsumerOperation;
using dansandu::farseer::internal::windows::register_request_callback_operation::createRegisterRequestCallbackOperation;
using dansandu::farseer::internal::windows::send_bytes_operation::createSendBytesOperation;
using dansandu::farseer::internal::windows::send_request_operation::createSendRequestOperation;
using dansandu::journey::exception::WideException;

namespace dansandu::farseer::internal::windows::operation_scheduler
{

namespace
{

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
      thread_{&OperationScheduler::consumeOperations, this}
{
}

OperationScheduler::~OperationScheduler() noexcept
{
    scheduleAbortOperation();

    thread_.join();

    sockets_.clear();

    ::CloseHandle(completionPort_);
}

HANDLE OperationScheduler::getCompletionPort()
{
    return completionPort_;
}

Socket& OperationScheduler::insertSocket(const SocketIdentifier socketIdentifier, Socket&& socket)
{
    const auto [position, inserted] = sockets_.emplace(socketIdentifier, std::move(socket));

    if (!inserted)
    {
        THROW(
            std::logic_error, "Couldn't insert socket with ID ", socketIdentifier,
            " because the ID is used by another socket"
        );
    }

    return position->second;
}

Socket& OperationScheduler::getSocketOrThrow(const SocketIdentifier socketIdentifier)
{
    const auto position = sockets_.find(socketIdentifier);

    if (position != sockets_.end())
    {
        return position->second;
    }

    WTHROW(InternalSocketError, "Couldn't find socket with ID ", socketIdentifier);
}

void OperationScheduler::eraseSocket(const SocketIdentifier socketIdentifier)
{
    const auto position = sockets_.find(socketIdentifier);

    if (position != sockets_.end())
    {
        SCOPE_EXIT([&] { sockets_.erase(position); });

        const auto& socket = position->second;

        try
        {
            if (socket.listeningSocketIdentifier != invalidSocketIdentifier)
            {
                const auto listeningSocketPosition = sockets_.find(socket.listeningSocketIdentifier);

                if (listeningSocketPosition != sockets_.end())
                {
                    listeningSocketPosition->second.connectionCallback(SocketEvent::clientClosed, socketIdentifier);
                }
            }
            else
            {
                socket.connectionCallback(SocketEvent::serverClosed, socketIdentifier);
            }

            LOG_INFO(
                "Socket with ID ", socketIdentifier, " and address ", socket.socket.getIpAddress(), ":",
                socket.socket.getPort(), " was closed"
            );
        }
        catch (const WideException& wideException)
        {
            LOG_ERROR(
                "Wide exception was thrown while trying to close socket with ID ", socketIdentifier,
                " with message: ", wideException.getMessage()
            );
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR(
                "Exception was thrown while trying to close socket with ID ", socketIdentifier,
                " with message: ", exception.what()
            );
        }
        catch (...)
        {
            LOG_ERROR("Unknown exception was thrown while trying to close socket with ID ", socketIdentifier);
        }
    }
}

void OperationScheduler::scheduleOperation(std::unique_ptr<IOperation>&& operation)
{
    const auto name = operation->getName();

    const auto socketIdentifier = operation->getSocketIdentifier();

    const auto nonUserOperation = dynamic_cast<INonUserOperation*>(operation.get());

    if (nonUserOperation != nullptr)
    {
        nonUserOperation->schedule(*this);
    }
    else
    {
        const auto numberOfBytesTransferred = 0;

        const auto postResult = ::PostQueuedCompletionStatus(
            completionPort_, numberOfBytesTransferred, defaultCompletionKey, operation.get()
        );

        if (!postResult)
        {
            THROW(
                std::runtime_error, "Scheduling ", name, " with socket ID ", socketIdentifier, " failed with error ",
                getLastErrorMessage()
            );
        }
    }

    // The completion port now owns the operation. Ownership is regained by calling GetQueuedCompletionStatus.
    operation.release();

    LOG_DEBUG("Scheduled ", name, " with socket ID ", socketIdentifier);
}

SocketIdentifier OperationScheduler::scheduleConnectOperation(
    const std::string& ipAddress, const int port,
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();

    scheduleOperation(createConnectOperation(socketIdentifier, ipAddress, port, std::move(connectionCallback)));

    return socketIdentifier;
}

SocketIdentifier OperationScheduler::scheduleListenOperation(
    const std::string& ipAddress, const int port,
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();

    scheduleOperation(createListenOperation(socketIdentifier, ipAddress, port, std::move(connectionCallback)));

    return socketIdentifier;
}

void OperationScheduler::scheduleAcceptOperation(const SocketIdentifier listeningSocketIdentifier)
{
    const auto pendingAcceptSocketIdentifier = socketIdentifierSequencer_.generate();

    scheduleOperation(createAcceptOperation(listeningSocketIdentifier, pendingAcceptSocketIdentifier));
}

void OperationScheduler::scheduleReceiveOperation(const SocketIdentifier socketIdentifier)
{
    scheduleOperation(createReceiveOperation(socketIdentifier));
}

void OperationScheduler::scheduleRegisterMessageConsumerOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<void(std::any&&)>&& messageConsumer
)
{
    scheduleOperation(
        createRegisterMessageConsumerOperation(socketIdentifier, protocolIdentifier, std::move(messageConsumer))
    );
}

void OperationScheduler::scheduleRegisterRequestCallbackOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<std::any(std::any&&)>&& requestConsumer
)
{
    scheduleOperation(
        createRegisterRequestCallbackOperation(socketIdentifier, protocolIdentifier, std::move(requestConsumer))
    );
}

void OperationScheduler::scheduleSendBytesOperation(
    const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes
)
{
    scheduleOperation(createSendBytesOperation(socketIdentifier, std::move(bytes)));
}

void OperationScheduler::scheduleSendRequestOperation(
    const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber protocolSequenceNumber,
    std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer
)
{
    scheduleOperation(createSendRequestOperation(
        socketIdentifier, protocolSequenceNumber, std::move(bytes), std::move(responseConsumer)
    ));
}

void OperationScheduler::scheduleCloseOperation(const SocketIdentifier socketIdentifier)
{
    scheduleOperation(createCloseOperation(socketIdentifier));
}

void OperationScheduler::scheduleAbortOperation()
{
    const auto numberOfBytesTransferred = 0;
    const auto overlapped = LPOVERLAPPED{nullptr};
    const auto postResult =
        ::PostQueuedCompletionStatus(completionPort_, numberOfBytesTransferred, defaultCompletionKey, overlapped);

    if (postResult == 0)
    {
        LOG_ERROR("Couldn't schedule AbortOperation to the queue: ", getLastErrorMessage());
    }
    else
    {
        LOG_DEBUG("Scheduled AbortOperation");
    }
}

void OperationScheduler::handleOperationExecutionFailure(
    const char* const name, const SocketIdentifier socketIdentifier, const std::wstring_view message
)
{
    if (message.empty())
    {
        LOG_ERROR(name, " with socket ID ", socketIdentifier, " failed and the socket will be erased");
    }
    else
    {
        LOG_ERROR(name, " with socket ID ", socketIdentifier, " failed and the socket will be erased: ", message);
    }

    eraseSocket(socketIdentifier);
}

bool OperationScheduler::handleSuccessfulOperation(
    const std::unique_ptr<IOperation>& operation, const DWORD numberOfBytesTransferred
)
{
    const auto name = operation->getName();

    const auto socketIdentifier = operation->getSocketIdentifier();

    LOG_DEBUG("Executing ", name, " with socket ID ", socketIdentifier);

    try
    {
        return operation->execute(*this, numberOfBytesTransferred);
    }
    catch (const WideException& exception)
    {
        handleOperationExecutionFailure(name, socketIdentifier, exception.getMessage());
    }
    catch (const std::exception& exception)
    {
        handleOperationExecutionFailure(name, socketIdentifier, toWideString(exception.what()));
    }
    catch (...)
    {
        handleOperationExecutionFailure(name, socketIdentifier);
    }

    return true;
}

void OperationScheduler::handleFailedOperation(std::unique_ptr<IOperation>&& operation, const DWORD errorCode)
{
    const auto name = operation->getName();
    const auto socketIdentifier = operation->getSocketIdentifier();
    const auto level = operation->getLoggingLevelFromErrorCode(errorCode);
    const auto message = getErrorMessageFromCode(errorCode);

    LOG(level, name, " with socket ID ", socketIdentifier, " failed and the socket will be erased: ", message);

    eraseSocket(socketIdentifier);
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
                LOG_DEBUG("Received AbortOperation");
                return;
            }

            const auto name = operation->getName();

            const auto socketIdentifier = operation->getSocketIdentifier();

            const auto discard = handleSuccessfulOperation(operation, numberOfBytesTransferred);

            if (discard)
            {
                operation.reset();

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

            handleFailedOperation(std::move(operation), errorCode);
        }
    }
}

void OperationScheduler::consumeOperations()
{
    LOG_DEBUG("Started operations consumer thread");

    try
    {
        consumeOperationsWork();

        LOG_DEBUG("Gracefully exited operations consumer thread");
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
}

}
#endif
