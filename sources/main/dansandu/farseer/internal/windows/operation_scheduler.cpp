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
using dansandu::farseer::internal::windows::i_operation_scheduler::defaultCompletionKey;
using dansandu::farseer::internal::windows::i_operation_scheduler::IOperationScheduler;
using dansandu::farseer::internal::windows::i_operation_scheduler::Operation;
using dansandu::farseer::internal::windows::i_operation_scheduler::Socket;
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

SocketIdentifier OperationScheduler::scheduleConnectOperation(const std::string& ipAddress, const int port,
                                                              ConnectionCallback&& connectionCallback)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();
    operationContainer_.insert(createConnectOperation(socketIdentifier, ipAddress, port, std::move(connectionCallback)),
                               *this);
    return socketIdentifier;
}

SocketIdentifier OperationScheduler::scheduleListenOperation(const std::string& ipAddress, const int port,
                                                             ConnectionCallback&& connectionCallback)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();
    operationContainer_.insert(createListenOperation(socketIdentifier, ipAddress, port, std::move(connectionCallback)),
                               *this);
    return socketIdentifier;
}

void OperationScheduler::scheduleAcceptOperation(const SocketIdentifier listeningSocketIdentifier)
{
    const auto pendingAcceptSocketIdentifier = socketIdentifierSequencer_.generate();
    operationContainer_.insert(createAcceptOperation(pendingAcceptSocketIdentifier, listeningSocketIdentifier), *this);
}

void OperationScheduler::scheduleReceiveOperation(const SocketIdentifier socketIdentifier)
{
    operationContainer_.insert(createReceiveOperation(socketIdentifier), *this);
}

void OperationScheduler::scheduleRegisterMessageConsumerOperation(const SocketIdentifier socketIdentifier,
                                                                  const ProtocolIdentifier protocolIdentifier,
                                                                  UniqueFunction<void(std::any&&)>&& messageConsumer)
{
    operationContainer_.insert(
        createRegisterMessageConsumerOperation(socketIdentifier, protocolIdentifier, std::move(messageConsumer)),
        *this);
}

void OperationScheduler::scheduleRegisterRequestCallbackOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<std::any(std::any&&)>&& requestConsumer)
{
    operationContainer_.insert(
        createRegisterRequestCallbackOperation(socketIdentifier, protocolIdentifier, std::move(requestConsumer)),
        *this);
}

void OperationScheduler::scheduleSendBytesOperation(const SocketIdentifier socketIdentifier,
                                                    std::vector<uint8_t>&& bytes)
{
    operationContainer_.insert(createSendBytesOperation(socketIdentifier, std::move(bytes)), *this);
}

void OperationScheduler::scheduleSendRequestOperation(const SocketIdentifier socketIdentifier,
                                                      const ProtocolSequenceNumber protocolSequenceNumber,
                                                      std::vector<uint8_t>&& bytes,
                                                      UniqueFunction<void(std::any&&)>&& expectedResponseConsumer)
{
    operationContainer_.insert(createSendRequestOperation(socketIdentifier, protocolSequenceNumber, std::move(bytes),
                                                          std::move(expectedResponseConsumer)),
                               *this);
}

void OperationScheduler::scheduleCloseOperation(const SocketIdentifier socketIdentifier)
{
    operationContainer_.insert(createCloseOperation(socketIdentifier), *this);
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

        if (dequeueResult == TRUE)
        {
            if (completionKey == defaultCompletionKey && overlapped == nullptr)
            {
                LOG_DEBUG("Received abort operation");

                break;
            }

            operationContainer_.handleSuccessfulOperation(overlapped, numberOfBytesTransferred, *this);
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

            operationContainer_.handleFailedOperation(overlapped, errorCode);
        }
    }

    LOG_DEBUG("Exiting operations consumer thread");
}

}
#endif
