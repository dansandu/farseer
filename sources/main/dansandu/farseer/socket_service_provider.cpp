#include "dansandu/farseer/socket_service_provider.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/error.hpp"
#include "dansandu/farseer/internal/internal_socket_service_exception.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/socket_service_operation.hpp"
#include "dansandu/farseer/internal/tcp_socket.hpp"
#include "dansandu/farseer/internal/wsa_scope_guard.hpp"
#include "dansandu/journey/logging.hpp"

#include <mswsock.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using dansandu::farseer::internal::error::getLastErrorMessage;
using dansandu::farseer::internal::internal_socket_service_exception::InternalSocketServiceException;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::socket_service_operation::SocketServiceOperation;
using dansandu::farseer::internal::socket_service_operation::SocketServiceOperationContainer;
using dansandu::farseer::internal::socket_service_operation::SocketServiceOperationType;
using dansandu::farseer::internal::socket_service_operation::toString;
using dansandu::farseer::internal::tcp_socket::TcpSocket;
using dansandu::farseer::internal::wsa_scope_guard::WsaScopeGuard;

namespace dansandu::farseer::socket_service_provider
{

namespace
{

HANDLE initializeIoCompletionPort(const u_long initialCompletionKey)
{
    const auto handle = HANDLE{INVALID_HANDLE_VALUE};
    const auto existingCompletionPort = HANDLE{nullptr};
    const auto numberOfConcurrentThreads = DWORD{0};

    const auto completionPort =
        ::CreateIoCompletionPort(handle, existingCompletionPort, initialCompletionKey, numberOfConcurrentThreads);
    if (completionPort == nullptr)
    {
        THROW(std::runtime_error, "I/O completion port creation for the SocketServiceProvider failed with error ",
              getLastErrorMessage());
    }

    return completionPort;
}

DWORD WINAPI socketServiceProviderLoop(LPVOID parameter);

struct SocketServiceProviderImplementation
{
    SocketServiceProviderImplementation(const bool initializeWsa)
        : wsaScopeGuard{initializeWsa},
          initialCompletionKey{InvalidServiceId.integer()},
          completionPort{initializeIoCompletionPort(initialCompletionKey)},
          serviceIdSequenceKey{initialCompletionKey + 1},
          thread{nullptr}
    {
        // Wrap completionPort and thread with RAII. Move to separate headers in internal folder. Make thread const.

        auto threadId = DWORD{0};
        const auto threadAttributes = LPSECURITY_ATTRIBUTES{nullptr};
        const auto stackSize = 0;
        const auto threadFlags = DWORD{0};
        const auto threadParameter = static_cast<LPVOID>(this);
        thread = ::CreateThread(threadAttributes, stackSize, socketServiceProviderLoop, threadParameter, threadFlags,
                                &threadId);
        if (thread == nullptr)
        {
            ::CloseHandle(completionPort);
            THROW(std::runtime_error, "Thread creation for the SocketServiceProvider failed with error ",
                  getLastErrorMessage());
        }
    }

    ~SocketServiceProviderImplementation()
    {
        const auto numberOfBytesTransferred = 0;
        const auto overlapped = LPOVERLAPPED{nullptr};
        const auto postResult =
            ::PostQueuedCompletionStatus(completionPort, numberOfBytesTransferred, initialCompletionKey, overlapped);
        if (postResult == 0)
        {
            LOG_ERROR("Posting abort operation to SocketServiceProvider failed with error ", getLastErrorMessage());
        }
        else
        {
            LOG_INFO("Posted abort operation to SocketServiceProvider thread");
        }

        const auto waitTimeout = INFINITE;
        ::WaitForSingleObject(thread, waitTimeout);
        ::CloseHandle(thread);
        ::CloseHandle(completionPort);
    }

    const WsaScopeGuard wsaScopeGuard;
    const u_long initialCompletionKey;
    const HANDLE completionPort;
    std::atomic<SocketServiceId::IntegerType> serviceIdSequenceKey;
    SocketServiceOperationContainer operations;
    HANDLE thread;
};

struct SocketService
{
    TcpSocket socket;
    SocketServiceId listeningServiceId;
    CallbackType callback;
    ProtocolReader protocolReader;
};

using SocketServiceIterator = std::map<SocketServiceId, SocketService>::iterator;

SocketServiceIterator getServiceOrThrow(std::map<SocketServiceId, SocketService>& services,
                                        const SocketServiceId serviceId)
{
    if (const auto servicePosition = services.find(serviceId); servicePosition != services.end())
    {
        return servicePosition;
    }
    WTHROW(InternalSocketServiceException, "Couldn't find service with ID ", serviceId.integer());
}

void invokeSocketBytesReceivedCallback(std::map<SocketServiceId, SocketService>& services,
                                       const SocketServiceIterator servicePosition, BytesType bytes)
{
    const auto& tcpSocket = servicePosition->second.socket;

    LOG_INFO("Socket with ID ", servicePosition->first.integer(), " and address ", tcpSocket.getIpAddress(), ':',
             tcpSocket.getPort(), " received ", bytes.size(), " bytes");

    if (servicePosition->second.listeningServiceId != InvalidServiceId)
    {
        const auto listeningServicePosition = getServiceOrThrow(services, servicePosition->second.listeningServiceId);

        listeningServicePosition->second.protocolReader.read(bytes);
    }
    else
    {
        servicePosition->second.protocolReader.read(bytes);
    }
}

void invokeSocketClosedCallback(std::map<SocketServiceId, SocketService>& services,
                                const SocketServiceIterator servicePosition)
{
    const auto& tcpSocket = servicePosition->second.socket;

    LOG_INFO("Socket with ID ", servicePosition->first.integer(), " and address ", tcpSocket.getIpAddress(), ':',
             tcpSocket.getPort(), " was closed");

    if (servicePosition->second.listeningServiceId != InvalidServiceId)
    {
        const auto listeningServicePosition = getServiceOrThrow(services, servicePosition->second.listeningServiceId);

        listeningServicePosition->second.callback(SocketServiceEvent::clientClosed, listeningServicePosition->first,
                                                  servicePosition->first, {});
    }
    else
    {
        servicePosition->second.callback(SocketServiceEvent::serverClosed, InvalidServiceId, servicePosition->first,
                                         {});
    }

    services.erase(servicePosition->first);
}

void postAcceptOperation(SocketServiceProviderImplementation* impl, std::map<SocketServiceId, SocketService>& services,
                         const SocketServiceIterator listeningServicePosition)
{
    const auto pendingAcceptServiceId = SocketServiceId{impl->serviceIdSequenceKey++};

    const auto pendingAcceptOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::pendingAccept,
        .serviceId = pendingAcceptServiceId,
    });

    auto pendingAcceptSocket =
        listeningServicePosition->second.socket.postAccept(impl->completionPort, pendingAcceptOperation);

    const auto [pendingAcceptServicePosition, pendingAcceptServiceInserted] = services.insert(
        {pendingAcceptServiceId,
         {.socket = std::move(pendingAcceptSocket), .listeningServiceId = listeningServicePosition->first}});

    if (!pendingAcceptServiceInserted)
    {
        THROW(std::logic_error, "Couldn't open accepting service with ID ", pendingAcceptServiceId.integer(),
              " because the ID is used by another service");
    }
}

void handleListenOperation(SocketServiceProviderImplementation* impl,
                           std::map<SocketServiceId, SocketService>& services,
                           std::unique_ptr<SocketServiceOperation> listenOperation)
{
    const auto listeningServiceId = listenOperation->serviceId;

    auto listeningSocket = TcpSocket{impl->completionPort, listeningServiceId};

    listeningSocket.listen(std::move(listenOperation->ipAddress), listenOperation->port);

    const auto [listeningServicePosition, listeningServiceInserted] =
        services.insert({listeningServiceId,
                         {
                             .socket = std::move(listeningSocket),
                             .listeningServiceId = InvalidServiceId,
                             .callback = std::move(listenOperation->callback),
                         }});

    if (!listeningServiceInserted)
    {
        THROW(std::logic_error, "Couldn't open listening service with ID ", listeningServiceId.integer(),
              " because the ID is used by another service");
    }

    postAcceptOperation(impl, services, listeningServicePosition);

    listeningServicePosition->second.callback(SocketServiceEvent::serverOpen, listeningServiceId, InvalidServiceId, {});

    LOG_INFO("Opened listening socket with ID ", listeningServiceId.integer(), " address ",
             listeningServicePosition->second.socket.getIpAddress(), ':',
             listeningServicePosition->second.socket.getPort());
}

void postReceiveBytesOperation(SocketServiceProviderImplementation* impl,
                               std::map<SocketServiceId, SocketService>& services,
                               const SocketServiceIterator servicePosition)
{
    const auto pendingReceiveBytesOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::pendingReceiveBytes,
        .serviceId = servicePosition->first,
    });

    servicePosition->second.socket.postReceive(pendingReceiveBytesOperation);
}

void handleFinishedAcceptOperation(SocketServiceProviderImplementation* impl,
                                   std::map<SocketServiceId, SocketService>& services,
                                   std::unique_ptr<SocketServiceOperation> finishedAcceptOperation)
{
    const auto acceptedServicePosition = getServiceOrThrow(services, finishedAcceptOperation->serviceId);

    const auto listeningServicePosition =
        getServiceOrThrow(services, acceptedServicePosition->second.listeningServiceId);

    auto& acceptedSocket = acceptedServicePosition->second.socket;

    acceptedSocket.accept(listeningServicePosition->second.socket);

    postAcceptOperation(impl, services, listeningServicePosition);

    postReceiveBytesOperation(impl, services, acceptedServicePosition);

    listeningServicePosition->second.callback(SocketServiceEvent::clientOpen, listeningServicePosition->first,
                                              acceptedServicePosition->first, {});

    LOG_INFO("Accepted client socket with address ", acceptedSocket.getIpAddress(), ':', acceptedSocket.getPort());
}

void handleConnectOperation(SocketServiceProviderImplementation* impl,
                            std::map<SocketServiceId, SocketService>& services,
                            std::unique_ptr<SocketServiceOperation> connectOperation)
{
    const auto pendingConnectServiceId = connectOperation->serviceId;

    auto pendingConnectSocket = TcpSocket{impl->completionPort, pendingConnectServiceId};

    // Reuse original send operation if possible
    const auto pendingConnectOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::pendingConnect,
        .serviceId = pendingConnectServiceId,
    });

    pendingConnectSocket.postConnect(std::move(connectOperation->ipAddress), connectOperation->port,
                                     pendingConnectOperation);

    const auto [pendingConnectPosition, pendingConnectServiceInserted] =
        services.insert({pendingConnectServiceId,
                         {
                             .socket = std::move(pendingConnectSocket),
                             .listeningServiceId = InvalidServiceId,
                             .callback = std::move(connectOperation->callback),
                         }});

    if (!pendingConnectServiceInserted)
    {
        THROW(std::logic_error, "Couldn't open connection service with ID ", pendingConnectServiceId.integer(),
              " because the ID is used by another service");
    }
}

void handleFinishedConnectOperation(SocketServiceProviderImplementation* impl,
                                    std::map<SocketServiceId, SocketService>& services,
                                    std::unique_ptr<SocketServiceOperation> finishedConnectOperation)
{
    const auto connectedServicePosition = getServiceOrThrow(services, finishedConnectOperation->serviceId);

    auto& connectedSocket = connectedServicePosition->second.socket;

    connectedSocket.connect();

    connectedServicePosition->second.callback(SocketServiceEvent::clientOpen, InvalidServiceId,
                                              connectedServicePosition->first, {});

    postReceiveBytesOperation(impl, services, connectedServicePosition);

    LOG_INFO("Connected to socket with address ", connectedSocket.getIpAddress(), ':', connectedSocket.getPort());
}

void handleFinishedReceiveBytesOperation(SocketServiceProviderImplementation* impl,
                                         std::map<SocketServiceId, SocketService>& services,
                                         DWORD numberOfBytesTransferred,
                                         std::unique_ptr<SocketServiceOperation> finishedReceiveOperation)
{
    const auto servicePosition = getServiceOrThrow(services, finishedReceiveOperation->serviceId);

    if (numberOfBytesTransferred > 0)
    {
        auto receivedBytes =
            BytesType(finishedReceiveOperation->buffer, finishedReceiveOperation->buffer + numberOfBytesTransferred);

        invokeSocketBytesReceivedCallback(services, servicePosition, std::move(receivedBytes));

        postReceiveBytesOperation(impl, services, servicePosition);
    }
    else
    {
        invokeSocketClosedCallback(services, servicePosition);
    }
}

void handleSendBytesOperation(SocketServiceProviderImplementation* impl,
                              std::map<SocketServiceId, SocketService>& services,
                              std::unique_ptr<SocketServiceOperation> sendBytesOperation)
{
    const auto servicePosition = getServiceOrThrow(services, sendBytesOperation->serviceId);

    // Reuse original send operation if possible
    const auto pendingSendBytesOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::pendingSendBytes,
        .serviceId = sendBytesOperation->serviceId,
        .bytes = std::move(sendBytesOperation->bytes),
    });

    servicePosition->second.socket.postSend(pendingSendBytesOperation);
}

void handleFinishedSendBytesOperation(std::unique_ptr<SocketServiceOperation> finishedSendBytesOperation)
{
    LOG_INFO("Sent bytes to service ID ", finishedSendBytesOperation->serviceId.integer());
}

void handleRegisterMessageConsumerOperation(std::map<SocketServiceId, SocketService>& services,
                                            std::unique_ptr<SocketServiceOperation> registerMessageOperation)
{
    // register to listening socket or to accepted socket
    const auto socketServicePosition = getServiceOrThrow(services, registerMessageOperation->serviceId);

    socketServicePosition->second.protocolReader.registerProtocolConsumer(
        registerMessageOperation->protocolIdentifier, std::move(registerMessageOperation->messageConsumer));

    LOG_INFO("Registered consumer for message with ID ", registerMessageOperation->protocolIdentifier.getValue(),
             " and socket service ID ", registerMessageOperation->serviceId.integer());
}

DWORD WINAPI socketServiceProviderLoop(LPVOID parameter)
{
    const auto impl = static_cast<SocketServiceProviderImplementation*>(parameter);

    auto services = std::map<SocketServiceId, SocketService>{};

    while (true)
    {
        auto numberOfBytesTransferred = DWORD{0};
        unsigned long long completionKey = 0;
        auto overlapped = LPOVERLAPPED{nullptr};
        auto timeout = INFINITE;
        const auto completionStatusResult = ::GetQueuedCompletionStatus(impl->completionPort, &numberOfBytesTransferred,
                                                                        &completionKey, &overlapped, timeout);

        if (completionStatusResult == TRUE)
        {
            if (completionKey == impl->initialCompletionKey && overlapped == nullptr)
            {
                LOG_INFO("Closing SocketServiceProvider thread");
                return 0;
            }

            auto operation = impl->operations.pop(overlapped);
            if (!operation)
            {
                LOG_ERROR("Unknown successful operation dequeued from completion queue with memory address: ",
                          overlapped);
                continue;
            }

            try
            {
                const auto operationType = operation->operationType;
                switch (operationType)
                {
                case SocketServiceOperationType::listen:
                    handleListenOperation(impl, services, std::move(operation));
                    break;
                case SocketServiceOperationType::pendingAccept:
                    handleFinishedAcceptOperation(impl, services, std::move(operation));
                    break;
                case SocketServiceOperationType::connect:
                    handleConnectOperation(impl, services, std::move(operation));
                    break;
                case SocketServiceOperationType::pendingConnect:
                    handleFinishedConnectOperation(impl, services, std::move(operation));
                    break;
                case SocketServiceOperationType::pendingReceiveBytes:
                    handleFinishedReceiveBytesOperation(impl, services, numberOfBytesTransferred, std::move(operation));
                    break;
                case SocketServiceOperationType::sendBytes:
                    handleSendBytesOperation(impl, services, std::move(operation));
                    break;
                case SocketServiceOperationType::pendingSendBytes:
                    handleFinishedSendBytesOperation(std::move(operation));
                    break;
                case SocketServiceOperationType::registerMessageConsumer:
                    handleRegisterMessageConsumerOperation(services, std::move(operation));
                    break;
                case SocketServiceOperationType::close:
                    break;
                default:
                    LOG_ERROR("Unknown SocketServiceOperationType");
                    break;
                }
            }
            catch (const InternalSocketServiceException& exception)
            {
                LOG_ERROR(exception.getMessage());
            }
            catch (const std::exception& exception)
            {
                LOG_CRITICAL("Closing SocketServiceProvider thread due to critial error: ", exception.what());
                return 0;
            }
        }
        else
        {
            if (overlapped == nullptr)
            {
                LOG_ERROR("Could not dequeue operation from completion queue -- closing SocketServiceProvider thread");
                return 0;
            }

            const auto operation = impl->operations.pop(overlapped);

            if (!operation)
            {
                LOG_ERROR("Unknown failed operation dequeued from completion queue with memory address: ", overlapped);
            }
            else
            {
                LOG_ERROR("Operation ", toString(operation->operationType), " with service ID ",
                          operation->serviceId.integer(), " failed with error ", getLastErrorMessage());
            }
        }
    }

    return 0;
}

}

SocketServiceProvider::SocketServiceProvider(bool initializeWsa)
    : implementation_{std::make_shared<SocketServiceProviderImplementation>(initializeWsa)}
{
}

SocketServiceProvider::~SocketServiceProvider()
{
}

void SocketServiceProvider::listen(std::wstring ipAddress, const int port, CallbackType callback) const
{
    const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

    const auto listenOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::listen,
        .serviceId = SocketServiceId{impl->serviceIdSequenceKey++},
        .ipAddress = std::move(ipAddress),
        .port = port,
        .callback = std::move(callback),
    });

    const auto numberOfBytesTransferred = 0;
    const auto postResult = ::PostQueuedCompletionStatus(impl->completionPort, numberOfBytesTransferred,
                                                         impl->initialCompletionKey, listenOperation);
    if (postResult == 0)
    {
        THROW(std::runtime_error, "Posting listen operation to SocketServiceProvider failed with error ",
              getLastErrorMessage());
    }
}

void SocketServiceProvider::connect(std::wstring ipAddress, const int port, CallbackType callback) const
{
    const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

    const auto connectOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::connect,
        .serviceId = SocketServiceId{impl->serviceIdSequenceKey++},
        .ipAddress = std::move(ipAddress),
        .port = std::move(port),
        .callback = std::move(callback),
    });

    const auto numberOfBytesTransferred = 0;
    const auto postResult = ::PostQueuedCompletionStatus(impl->completionPort, numberOfBytesTransferred,
                                                         impl->initialCompletionKey, connectOperation);
    if (postResult == 0)
    {
        THROW(std::runtime_error, "Posting connect operation to SocketServiceProvider failed with error ",
              getLastErrorMessage());
    }
}

void SocketServiceProvider::sendBytes(const SocketServiceId serviceId, std::vector<uint8_t> bytes) const
{
    if (serviceId == InvalidServiceId)
    {
        THROW(std::logic_error, "Cannot send bytes using an InvalidServiceId");
    }

    const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

    const auto sendBytesOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::sendBytes,
        .serviceId = serviceId,
        .bytes = std::move(bytes),
    });

    const auto numberOfBytesTransferred = 0;
    const auto postResult = ::PostQueuedCompletionStatus(impl->completionPort, numberOfBytesTransferred,
                                                         impl->initialCompletionKey, sendBytesOperation);
    if (postResult == 0)
    {
        THROW(std::runtime_error, "Posting send bytes operation to SocketServiceProvider failed with error ",
              getLastErrorMessage());
    }
}

void SocketServiceProvider::registerMessageConsumer(const SocketServiceId serviceId,
                                                    const ProtocolIdentifier protocolIdentifier,
                                                    std::function<void(std::any)> messageConsumer) const
{
    if (serviceId == InvalidServiceId)
    {
        THROW(std::logic_error, "Cannot send bytes using an InvalidServiceId");
    }

    const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

    const auto registerMessageOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::registerMessageConsumer,
        .serviceId = serviceId,
        .protocolIdentifier = protocolIdentifier,
        .messageConsumer = std::move(messageConsumer),
    });

    const auto numberOfBytesTransferred = 0;
    const auto postResult = ::PostQueuedCompletionStatus(impl->completionPort, numberOfBytesTransferred,
                                                         impl->initialCompletionKey, registerMessageOperation);
    if (postResult == 0)
    {
        THROW(std::runtime_error, "Posting register message operation to SocketServiceProvider failed with error ",
              getLastErrorMessage());
    }
}

void SocketServiceProvider::close(const SocketServiceId serviceId) const
{
    if (serviceId == InvalidServiceId)
    {
        THROW(std::logic_error, "Cannot close service with the InvalidServiceId");
    }
}

}
