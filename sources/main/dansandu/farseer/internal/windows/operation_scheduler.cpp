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

using dansandu::ballotin::string::toWideString;
using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::windows::accept_operation::createAcceptOperation;
using dansandu::farseer::internal::windows::close_operation::createCloseOperation;
using dansandu::farseer::internal::windows::connect_operation::createConnectOperation;
using dansandu::farseer::internal::windows::error::getErrorMessageFromCode;
using dansandu::farseer::internal::windows::error::getLastErrorCode;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::listen_operation::createListenOperation;
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
      socketIdentifierSequencer_{defaultCompletionKey + 1},
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
        THROW(std::logic_error, "Couldn't insert socket with ID ", socketIdentifier.getUnderlying(),
              " because the ID is used by another socket");
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

    WTHROW(InternalSocketError, "Couldn't find socket with ID ", socketIdentifier.getUnderlying());
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
        }
        catch (const WideException& wideException)
        {
            LOG_ERROR("Wide exception was thrown while trying to close socket with message: ",
                      wideException.getMessage());
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR("Exception was thrown while trying to close socket with message: ", exception.what());
        }

        LOG_INFO("Socket with ID ", socketIdentifier.getUnderlying(), " and address ", socket.socket.getIpAddress(),
                 ':', socket.socket.getPort(), " was closed");
    }
}

void OperationScheduler::insertOperation(std::unique_ptr<Operation>&& operation)
{
    const auto overlapped = operation->getOverlapped();
    const auto name = operation->getName();
    const auto socketIdentifier = operation->getSocketIdentifier();

    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto [position, inserted] = operations_.insert({overlapped, std::move(operation)});

    if (!inserted)
    {
        THROW(std::logic_error, "Couldn't push ", name, " with socket ID ", socketIdentifier.getUnderlying(),
              " because operation already exists");
    }

    SCOPE_FAILURE([&]() { operations_.erase(position); });

    position->second->postToCompletionPort(*this);

    LOG_DEBUG("Inserted ", name, " with socket ID ", socketIdentifier.getUnderlying());
}

SocketIdentifier OperationScheduler::scheduleConnectOperation(const std::string& ipAddress, const int port,
                                                              ConnectionCallback&& connectionCallback)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();
    insertOperation(createConnectOperation(socketIdentifier, ipAddress, port, std::move(connectionCallback)));
    return socketIdentifier;
}

SocketIdentifier OperationScheduler::scheduleListenOperation(const std::string& ipAddress, const int port,
                                                             ConnectionCallback&& connectionCallback)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();
    insertOperation(createListenOperation(socketIdentifier, ipAddress, port, std::move(connectionCallback)));
    return socketIdentifier;
}

void OperationScheduler::scheduleAcceptOperation(const SocketIdentifier listeningSocketIdentifier)
{
    const auto pendingAcceptSocketIdentifier = socketIdentifierSequencer_.generate();
    insertOperation(createAcceptOperation(pendingAcceptSocketIdentifier, listeningSocketIdentifier));
}

void OperationScheduler::scheduleReceiveOperation(const SocketIdentifier socketIdentifier)
{
    insertOperation(createReceiveOperation(socketIdentifier));
}

void OperationScheduler::scheduleRegisterMessageConsumerOperation(const SocketIdentifier socketIdentifier,
                                                                  const ProtocolIdentifier protocolIdentifier,
                                                                  UniqueFunction<void(std::any&&)>&& messageConsumer)
{
    insertOperation(
        createRegisterMessageConsumerOperation(socketIdentifier, protocolIdentifier, std::move(messageConsumer)));
}

void OperationScheduler::scheduleRegisterRequestCallbackOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<std::any(std::any&&)>&& requestConsumer)
{
    insertOperation(
        createRegisterRequestCallbackOperation(socketIdentifier, protocolIdentifier, std::move(requestConsumer)));
}

void OperationScheduler::scheduleSendBytesOperation(const SocketIdentifier socketIdentifier,
                                                    std::vector<uint8_t>&& bytes)
{
    insertOperation(createSendBytesOperation(socketIdentifier, std::move(bytes)));
}

void OperationScheduler::scheduleSendRequestOperation(const SocketIdentifier socketIdentifier,
                                                      const ProtocolSequenceNumber protocolSequenceNumber,
                                                      std::vector<uint8_t>&& bytes,
                                                      UniqueFunction<void(std::any&&)>&& expectedResponseConsumer)
{
    insertOperation(createSendRequestOperation(socketIdentifier, protocolSequenceNumber, std::move(bytes),
                                               std::move(expectedResponseConsumer)));
}

void OperationScheduler::scheduleCloseOperation(const SocketIdentifier socketIdentifier)
{
    insertOperation(createCloseOperation(socketIdentifier));
}

void OperationScheduler::scheduleAbortOperation()
{
    const auto numberOfBytesTransferred = 0;
    const auto overlapped = LPOVERLAPPED{nullptr};
    const auto postResult =
        ::PostQueuedCompletionStatus(completionPort_, numberOfBytesTransferred, defaultCompletionKey, overlapped);

    if (postResult == 0)
    {
        LOG_ERROR("Couldn't post abort operation to queue: ", getLastErrorMessage());
    }
    else
    {
        LOG_DEBUG("Posted abort operation to queue");
    }
}

void OperationScheduler::consumeOperations()
{
    LOG_DEBUG("Started operations consumer thread");

    while (true)
    {
        auto numberOfBytesTransferred = DWORD{0};
        auto completionKey = 0ULL;
        auto overlapped = LPOVERLAPPED{nullptr};
        auto timeout = INFINITE;

        const auto dequeueResult = ::GetQueuedCompletionStatus(completionPort_, &numberOfBytesTransferred,
                                                               &completionKey, &overlapped, timeout);

        try
        {
            if (dequeueResult == TRUE)
            {
                if (completionKey == defaultCompletionKey && overlapped == nullptr)
                {
                    LOG_DEBUG("Received abort operation");

                    break;
                }

                handleSuccessfulOperation(overlapped, numberOfBytesTransferred);
            }
            else
            {
                const auto errorCode = getLastErrorCode();

                if (overlapped == nullptr)
                {
                    const auto errorMessage = getErrorMessageFromCode(errorCode);

                    LOG_ERROR("Could not dequeue operation from completion queue: ", errorMessage);

                    break;
                }

                handleFailedOperation(overlapped, errorCode);
            }
        }
        catch (const WideException& exception)
        {
            handleFailedOperation(overlapped, exception.getMessage());
        }
        catch (const std::exception& exception)
        {
            handleFailedOperation(overlapped, toWideString(exception.what()));
        }
    }

    LOG_DEBUG("Exiting operations consumer thread");
}

void OperationScheduler::handleSuccessfulOperation(const LPWSAOVERLAPPED overlapped,
                                                   const DWORD numberOfBytesTransferred)
{
    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto position = operations_.find(overlapped);

    if (position != operations_.cend())
    {
        const auto name = position->second->getName();
        const auto socketIdentifier = position->second->getSocketIdentifier();

        LOG_DEBUG("Executing ", name, " with socket ID ", socketIdentifier.getUnderlying());

        const auto popOperation = position->second->finalize(*this, numberOfBytesTransferred);

        if (popOperation)
        {
            operations_.erase(position);

            LOG_DEBUG("Erased ", name, " with socket ID ", socketIdentifier.getUnderlying());
        }
        else
        {
            LOG_DEBUG("Keeping ", name, " with socket ID ", socketIdentifier.getUnderlying());
        }
    }
    else
    {
        THROW(std::logic_error, "Couldn't execute unknown operation");
    }
}

void OperationScheduler::handleFailedOperation(const LPWSAOVERLAPPED overlapped, const DWORD errorCode)
{
    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto position = operations_.find(overlapped);

    if (position != operations_.cend())
    {
        SCOPE_EXIT([&] { operations_.erase(position); });

        const auto name = position->second->getName();
        const auto socketIdentifier = position->second->getSocketIdentifier().getUnderlying();
        const auto level = position->second->getSystemErrorCodeLevel(errorCode);
        const auto message = getErrorMessageFromCode(errorCode);

        LOG(level, name, " with socket ID ", socketIdentifier, " failed: ", message);
    }
    else
    {
        const auto errorMessage = getErrorMessageFromCode(errorCode);

        LOG_ERROR("Unknown operation failed: ", errorMessage);
    }
}

void OperationScheduler::handleFailedOperation(const LPWSAOVERLAPPED overlapped, const std::wstring_view message)
{
    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto position = operations_.find(overlapped);

    if (position != operations_.cend())
    {
        SCOPE_EXIT([&] { operations_.erase(position); });

        const auto name = position->second->getName();
        const auto socketIdentifier = position->second->getSocketIdentifier().getUnderlying();

        if (message.empty())
        {
            LOG_ERROR(name, " with socket ID ", socketIdentifier, " failed");
        }
        else
        {
            LOG_ERROR(name, " with socket ID ", socketIdentifier, " failed: ", message);
        }
    }
    else
    {
        if (message.empty())
        {
            LOG_ERROR("Unknown operation failed");
        }
        else
        {
            LOG_ERROR("Unknown operation failed: ", message);
        }
    }
}

}
#endif
