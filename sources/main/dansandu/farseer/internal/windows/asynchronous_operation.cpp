#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/ballotin/string.hpp"
#include "dansandu/farseer/internal/windows/accept_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/close_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/connect_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/listen_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/receive_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/register_message_consumer_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/register_request_callback_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/send_bytes_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/send_request_asynchronous_operation.hpp"

using dansandu::ballotin::string::toWideString;
using dansandu::farseer::exception::InternalSocketServiceException;
using dansandu::farseer::internal::windows::error::getErrorMessageFromCode;
using dansandu::farseer::internal::windows::error::getLastErrorCode;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::journey::exception::WideException;

namespace dansandu::farseer::internal::windows::asynchronous_operation
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

AsynchronousOperationScheduler::AsynchronousOperationScheduler()
    : completionPort_{initializeIoCompletionPort()}, socketIdentifierSequencer_{defaultCompletionKey + 1}
{
}

AsynchronousOperationScheduler::~AsynchronousOperationScheduler() noexcept
{
    ::CloseHandle(completionPort_);
}

HANDLE AsynchronousOperationScheduler::getCompletionPort()
{
    return completionPort_;
}

Socket& AsynchronousOperationScheduler::insertSocket(const SocketIdentifier socketIdentifier, Socket&& socket)
{
    const auto [position, inserted] = sockets_.emplace(socketIdentifier, std::move(socket));

    if (!inserted)
    {
        THROW(std::logic_error, "Couldn't insert socket with ID ", socketIdentifier.getUnderlying(),
              " because the ID is used by another socket");
    }

    return position->second;
}

Socket& AsynchronousOperationScheduler::getSocketOrThrow(const SocketIdentifier socketIdentifier)
{
    const auto position = sockets_.find(socketIdentifier);

    if (position != sockets_.end())
    {
        return position->second;
    }

    WTHROW(InternalSocketServiceException, "Couldn't find socket with ID ", socketIdentifier.getUnderlying());
}

void AsynchronousOperationScheduler::eraseSocket(const SocketIdentifier socketIdentifier)
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

void AsynchronousOperationScheduler::insertOperation(std::unique_ptr<AsynchronousOperation> operation)
{
    const auto overlapped = operation->getOverlapped();
    const auto name = operation->getName();
    const auto socketIdentifier = operation->getSocketIdentifier();

    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto [position, inserted] = operations_.insert({overlapped, std::move(operation)});

    if (!inserted)
    {
        THROW(std::logic_error, "Couldn't push ", name, " with service ID ", socketIdentifier.getUnderlying(),
              " because operation already exists");
    }

    SCOPE_FAILURE([&]() { operations_.erase(position); });

    position->second->postToCompletionPort(*this);

    LOG_DEBUG("Inserted ", name, " with service ID ", socketIdentifier.getUnderlying());
}

SocketIdentifier
AsynchronousOperationScheduler::createConnectAsynchronousOperation(const std::wstring& ipAddress, const int port,
                                                                   ConnectionCallback&& connectionCallback)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();
    insertOperation(
        dansandu::farseer::internal::windows::connect_asynchronous_operation::createConnectAsynchronousOperation(
            socketIdentifier, ipAddress, port, std::move(connectionCallback)));
    return socketIdentifier;
}

SocketIdentifier
AsynchronousOperationScheduler::createListenAsynchronousOperation(const std::wstring& ipAddress, const int port,
                                                                  ConnectionCallback&& connectionCallback)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();
    insertOperation(
        dansandu::farseer::internal::windows::listen_asynchronous_operation::createListenAsynchronousOperation(
            socketIdentifier, ipAddress, port, std::move(connectionCallback)));
    return socketIdentifier;
}

void AsynchronousOperationScheduler::createAcceptAsynchronousOperation(const SocketIdentifier listeningSocketIdentifier)
{
    const auto pendingAcceptSocketIdentifier = socketIdentifierSequencer_.generate();
    insertOperation(
        dansandu::farseer::internal::windows::accept_asynchronous_operation::createAcceptAsynchronousOperation(
            pendingAcceptSocketIdentifier, listeningSocketIdentifier));
}

void AsynchronousOperationScheduler::createReceiveAsynchronousOperation(const SocketIdentifier socketIdentifier)
{
    insertOperation(
        dansandu::farseer::internal::windows::receive_asynchronous_operation::createReceiveAsynchronousOperation(
            socketIdentifier));
}

void AsynchronousOperationScheduler::createRegisterMessageConsumerAsynchronousOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<void(std::any&&)>&& messageConsumer)
{
    insertOperation(dansandu::farseer::internal::windows::register_message_consumer_asynchronous_operation::
                        createRegisterMessageConsumerAsynchronousOperation(socketIdentifier, protocolIdentifier,
                                                                           std::move(messageConsumer)));
}

void AsynchronousOperationScheduler::createRegisterRequestCallbackAsynchronousOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<std::any(std::any&&)>&& requestConsumer)
{
    insertOperation(dansandu::farseer::internal::windows::register_request_callback_asynchronous_operation::
                        createRegisterRequestCallbackAsynchronousOperation(socketIdentifier, protocolIdentifier,
                                                                           std::move(requestConsumer)));
}

void AsynchronousOperationScheduler::createSendBytesAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                                                          std::vector<uint8_t>&& bytes)
{
    insertOperation(
        dansandu::farseer::internal::windows::send_bytes_asynchronous_operation::createSendBytesAsynchronousOperation(
            socketIdentifier, std::move(bytes)));
}

void AsynchronousOperationScheduler::createSendRequestAsynchronousOperation(
    const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber protocolSequenceNumber,
    std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& expectedResponseConsumer)
{
    insertOperation(dansandu::farseer::internal::windows::send_request_asynchronous_operation::
                        createSendRequestAsynchronousOperation(socketIdentifier, protocolSequenceNumber,
                                                               std::move(bytes), std::move(expectedResponseConsumer)));
}

void AsynchronousOperationScheduler::createCloseAsynchronousOperation(const SocketIdentifier socketIdentifier)
{
    insertOperation(
        dansandu::farseer::internal::windows::close_asynchronous_operation::createCloseAsynchronousOperation(
            socketIdentifier));
}

void AsynchronousOperationScheduler::createAbortAsynchronousOperation()
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

bool AsynchronousOperationScheduler::waitAndConsumeAsynchronousOperation()
{
    auto numberOfBytesTransferred = DWORD{0};
    auto completionKey = 0ULL;
    auto overlapped = LPOVERLAPPED{nullptr};
    auto timeout = INFINITE;

    const auto dequeueResult =
        ::GetQueuedCompletionStatus(completionPort_, &numberOfBytesTransferred, &completionKey, &overlapped, timeout);

    try
    {
        if (dequeueResult == TRUE)
        {
            if (completionKey == defaultCompletionKey && overlapped == nullptr)
            {
                LOG_DEBUG("Received abort operation");

                return false;
            }

            handleSuccessfulAsynchronousOperation(overlapped, numberOfBytesTransferred);
        }
        else
        {
            const auto errorCode = getLastErrorCode();

            if (overlapped == nullptr)
            {
                const auto errorMessage = getErrorMessageFromCode(errorCode);

                LOG_ERROR("Could not dequeue operation from completion queue: ", errorMessage);

                return false;
            }

            handleFailedAsynchronousOperation(overlapped, errorCode);
        }
    }
    catch (const WideException& exception)
    {
        handleFailedAsynchronousOperation(overlapped, exception.getMessage());
    }
    catch (const std::exception& exception)
    {
        handleFailedAsynchronousOperation(overlapped, toWideString(exception.what()));
    }

    return true;
}

void AsynchronousOperationScheduler::handleSuccessfulAsynchronousOperation(const LPWSAOVERLAPPED overlapped,
                                                                           const DWORD numberOfBytesTransferred)
{
    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto position = operations_.find(overlapped);

    if (position != operations_.cend())
    {
        const auto name = position->second->getName();
        const auto socketIdentifier = position->second->getSocketIdentifier();

        LOG_DEBUG("Executing ", name, " with service ID ", socketIdentifier.getUnderlying());

        const auto popOperation = position->second->finalize(*this, numberOfBytesTransferred);

        if (popOperation)
        {
            operations_.erase(position);

            LOG_DEBUG("Erased ", name, " with service ID ", socketIdentifier.getUnderlying());
        }
        else
        {
            LOG_DEBUG("Keeping ", name, " with service ID ", socketIdentifier.getUnderlying());
        }
    }
    else
    {
        THROW(std::logic_error, "Couldn't execute unknown operation");
    }
}

void AsynchronousOperationScheduler::handleFailedAsynchronousOperation(const LPWSAOVERLAPPED overlapped,
                                                                       const DWORD errorCode)
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

        LOG(level, name, " with service ID ", socketIdentifier, " failed: ", message);
    }
    else
    {
        const auto errorMessage = getErrorMessageFromCode(errorCode);

        LOG_ERROR("Unknown operation failed: ", errorMessage);
    }
}

void AsynchronousOperationScheduler::handleFailedAsynchronousOperation(const LPWSAOVERLAPPED overlapped,
                                                                       const std::wstring_view message)
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
            LOG_ERROR(name, " with service ID ", socketIdentifier, " failed");
        }
        else
        {
            LOG_ERROR(name, " with service ID ", socketIdentifier, " failed: ", message);
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
