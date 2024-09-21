#include "dansandu/farseer/socket_service_provider.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/ballotin/logging.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/error.hpp"
#include "dansandu/farseer/internal/internal_socket_service_exception.hpp"
#include "dansandu/farseer/internal/socket_service_operation.hpp"
#include "dansandu/farseer/internal/tcp_socket.hpp"
#include "dansandu/farseer/internal/wsa_scope_guard.hpp"

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

using dansandu::ballotin::logging::LogCritical;
using dansandu::ballotin::logging::LogError;
using dansandu::ballotin::logging::LogInfo;
using dansandu::farseer::internal::error::getLastErrorMessage;
using dansandu::farseer::internal::internal_socket_service_exception::InternalSocketServiceException;
using dansandu::farseer::internal::socket_service_operation::SocketServiceOperation;
using dansandu::farseer::internal::socket_service_operation::SocketServiceOperationContainer;
using dansandu::farseer::internal::socket_service_operation::SocketServiceOperationType;
using dansandu::farseer::internal::socket_service_operation::toString;
using dansandu::farseer::internal::tcp_socket::TcpSocket;
using dansandu::farseer::internal::wsa_scope_guard::WsaScopeGuard;

namespace dansandu::farseer::socket_service_provider
{

static HANDLE initializeIoCompletionPort(const u_long initialCompletionKey)
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

static DWORD WINAPI socketServiceProviderLoop(LPVOID parameter);

struct SocketServiceProviderImplementation
{
    SocketServiceProviderImplementation(const bool initializeWsa)
        : wsaScopeGuard{initializeWsa},
          initialCompletionKey{InvalidServiceId.integer()},
          completionPort{initializeIoCompletionPort(initialCompletionKey)},
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
            LogError("Posting abort operation to SocketServiceProvider failed with error ", getLastErrorMessage());
        }

        LogInfo("Posted abort operation to SocketServiceProvider thread");

        const auto waitTimeout = INFINITE;
        ::WaitForSingleObject(thread, waitTimeout);
        ::CloseHandle(thread);
        ::CloseHandle(completionPort);
    }

    const WsaScopeGuard wsaScopeGuard;
    const u_long initialCompletionKey;
    const HANDLE completionPort;
    SocketServiceOperationContainer operations;
    HANDLE thread;
};

struct SocketService
{
    TcpSocket socket;
    SocketServiceId listeningServiceId;
    CallbackType callback;
};

using SocketServiceIterator = std::map<SocketServiceId, SocketService>::iterator;

static SocketServiceIterator getServiceOrThrow(std::map<SocketServiceId, SocketService>& services,
                                               const SocketServiceId serviceId)
{
    if (const auto servicePosition = services.find(serviceId); servicePosition != services.end())
    {
        return servicePosition;
    }
    WTHROW(InternalSocketServiceException, "Couldn't find service with ID ", serviceId.integer());
}

static void invokeSocketMessageReceivedCallback(std::map<SocketServiceId, SocketService>& services,
                                                const SocketServiceIterator servicePosition, BytesType&& message)
{
    const auto& tcpSocket = servicePosition->second.socket;

    LogInfo("Socket with ID ", servicePosition->first.integer(), " and address ", tcpSocket.getIpAddress(), ':',
            tcpSocket.getPort(), " received ", message.size(), " bytes");

    if (servicePosition->second.listeningServiceId != InvalidServiceId)
    {
        const auto listeningServicePosition = getServiceOrThrow(services, servicePosition->second.listeningServiceId);

        listeningServicePosition->second.callback(SocketServiceEvent::clientMessageReceived,
                                                  listeningServicePosition->first, servicePosition->first,
                                                  std::move(message));
    }
    else
    {
        servicePosition->second.callback(SocketServiceEvent::clientMessageReceived, InvalidServiceId,
                                         servicePosition->first, std::move(message));
    }
}

static void invokeSocketClosedCallback(std::map<SocketServiceId, SocketService>& services,
                                       const SocketServiceIterator servicePosition)
{
    const auto& tcpSocket = servicePosition->second.socket;

    LogInfo("Socket with ID ", servicePosition->first.integer(), " and address ", tcpSocket.getIpAddress(), ':',
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

static void postAcceptOperation(SocketServiceProviderImplementation* impl,
                                std::map<SocketServiceId, SocketService>& services,
                                SocketServiceId::IntegerType& sequenceKey,
                                const SocketServiceIterator listeningServicePosition)
{
    const auto pendingAcceptServiceId = SocketServiceId{sequenceKey++};

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

static void handleListenOperation(SocketServiceProviderImplementation* impl,
                                  std::map<SocketServiceId, SocketService>& services,
                                  SocketServiceId::IntegerType& sequenceKey,
                                  std::unique_ptr<SocketServiceOperation> listenOperation)
{
    const auto listeningServiceId = SocketServiceId{sequenceKey++};

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

    postAcceptOperation(impl, services, sequenceKey, listeningServicePosition);

    listeningServicePosition->second.callback(SocketServiceEvent::serverOpen, listeningServiceId, InvalidServiceId, {});

    LogInfo("Opened listening socket with ID ", listeningServiceId.integer(), " address ",
            listeningServicePosition->second.socket.getIpAddress(), ':',
            listeningServicePosition->second.socket.getPort());
}

static void postReceiveMessageOperation(SocketServiceProviderImplementation* impl,
                                        std::map<SocketServiceId, SocketService>& services,
                                        const SocketServiceIterator servicePosition)
{
    auto pendingReceiveMessageOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::pendingReceiveMessage,
        .serviceId = servicePosition->first,
    });

    servicePosition->second.socket.postReceive(pendingReceiveMessageOperation);
}

static void handlePendingAcceptOperation(SocketServiceProviderImplementation* impl,
                                         std::map<SocketServiceId, SocketService>& services,
                                         SocketServiceId::IntegerType& sequenceKey,
                                         std::unique_ptr<SocketServiceOperation> pendingAcceptOperation)
{
    const auto acceptedServicePosition = getServiceOrThrow(services, pendingAcceptOperation->serviceId);

    const auto listeningServicePosition =
        getServiceOrThrow(services, acceptedServicePosition->second.listeningServiceId);

    auto& acceptedSocket = acceptedServicePosition->second.socket;

    acceptedSocket.accept(listeningServicePosition->second.socket);

    postAcceptOperation(impl, services, sequenceKey, listeningServicePosition);

    postReceiveMessageOperation(impl, services, acceptedServicePosition);

    listeningServicePosition->second.callback(SocketServiceEvent::clientOpen, listeningServicePosition->first,
                                              acceptedServicePosition->first, {});

    LogInfo("Accepted client socket with address ", acceptedSocket.getIpAddress(), ':', acceptedSocket.getPort());
}

static void handleConnectOperation(SocketServiceProviderImplementation* impl,
                                   std::map<SocketServiceId, SocketService>& services,
                                   SocketServiceId::IntegerType& sequenceKey,
                                   std::unique_ptr<SocketServiceOperation> connectOperation)
{
    const auto pendingConnectServiceId = SocketServiceId{sequenceKey++};

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

static void handlePendingConnectOperation(SocketServiceProviderImplementation* impl,
                                          std::map<SocketServiceId, SocketService>& services,
                                          std::unique_ptr<SocketServiceOperation> pendingConnectOperation)
{
    const auto connectedServicePosition = getServiceOrThrow(services, pendingConnectOperation->serviceId);

    auto& connectedSocket = connectedServicePosition->second.socket;

    connectedSocket.connect();

    connectedServicePosition->second.callback(SocketServiceEvent::clientOpen, InvalidServiceId,
                                              connectedServicePosition->first, {});

    postReceiveMessageOperation(impl, services, connectedServicePosition);

    LogInfo("Connected to socket with address ", connectedSocket.getIpAddress(), ':', connectedSocket.getPort());
}

static void handlePendingReceiveMessageOperation(SocketServiceProviderImplementation* impl,
                                                 std::map<SocketServiceId, SocketService>& services,
                                                 DWORD numberOfBytesTransferred,
                                                 std::unique_ptr<SocketServiceOperation> pendingReceiveOperation)
{
    const auto servicePosition = getServiceOrThrow(services, pendingReceiveOperation->serviceId);

    if (numberOfBytesTransferred > 0)
    {
        auto receivedBytes =
            BytesType(pendingReceiveOperation->buffer, pendingReceiveOperation->buffer + numberOfBytesTransferred);

        invokeSocketMessageReceivedCallback(services, servicePosition, std::move(receivedBytes));

        postReceiveMessageOperation(impl, services, servicePosition);
    }
    else
    {
        invokeSocketClosedCallback(services, servicePosition);
    }
}

static void handleSendMessageOperation(SocketServiceProviderImplementation* impl,
                                       std::map<SocketServiceId, SocketService>& services,
                                       std::unique_ptr<SocketServiceOperation> sendMessageOperation)
{
    const auto servicePosition = getServiceOrThrow(services, sendMessageOperation->serviceId);

    // Reuse original send operation if possible
    const auto pendingSendMessageOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::pendingSendMessage,
        .serviceId = sendMessageOperation->serviceId,
        .message = std::move(sendMessageOperation->message),
    });

    servicePosition->second.socket.postSend(pendingSendMessageOperation);
}

static void handlePendingSendMessageOperation(std::unique_ptr<SocketServiceOperation> pendingSendMessageOperation)
{
    LogInfo("Sent message to service ID ", pendingSendMessageOperation->serviceId.integer());
}

DWORD WINAPI socketServiceProviderLoop(LPVOID parameter)
{
    const auto impl = static_cast<SocketServiceProviderImplementation*>(parameter);

    auto sequenceKey = SocketServiceId::IntegerType{impl->initialCompletionKey + 1};
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
                LogInfo("Closing SocketServiceProvider thread");
                return 0;
            }

            auto operation = impl->operations.pop(overlapped);
            if (!operation)
            {
                LogError("Unknown successful operation dequeued from completion queue with memory address: ",
                         overlapped);
                continue;
            }

            try
            {
                const auto operationType = operation->operationType;
                switch (operationType)
                {
                case SocketServiceOperationType::listen:
                    handleListenOperation(impl, services, sequenceKey, std::move(operation));
                    break;
                case SocketServiceOperationType::pendingAccept:
                    handlePendingAcceptOperation(impl, services, sequenceKey, std::move(operation));
                    break;
                case SocketServiceOperationType::connect:
                    handleConnectOperation(impl, services, sequenceKey, std::move(operation));
                    break;
                case SocketServiceOperationType::pendingConnect:
                    handlePendingConnectOperation(impl, services, std::move(operation));
                    break;
                case SocketServiceOperationType::pendingReceiveMessage:
                    handlePendingReceiveMessageOperation(impl, services, numberOfBytesTransferred,
                                                         std::move(operation));
                    break;
                case SocketServiceOperationType::sendMessage:
                    handleSendMessageOperation(impl, services, std::move(operation));
                    break;
                case SocketServiceOperationType::pendingSendMessage:
                    handlePendingSendMessageOperation(std::move(operation));
                    break;
                case SocketServiceOperationType::close:
                    break;
                default:
                    LogError("Unknown SocketServiceOperationType");
                    break;
                }
            }
            catch (const InternalSocketServiceException& exception)
            {
                LogError(exception.getMessage());
            }
            catch (const std::exception& exception)
            {
                LogCritical("Closing SocketServiceProvider thread due to critial error: ", exception.what());
                return 0;
            }
        }
        else
        {
            if (overlapped == nullptr)
            {
                LogError("Could not dequeue operation from completion queue -- closing SocketServiceProvider thread");
                return 0;
            }

            auto operation = impl->operations.pop(overlapped);
            if (!operation)
            {
                LogError("Unknown failed operation dequeued from completion queue with memory address: ", overlapped);
                continue;
            }

            LogError("Operation ", toString(operation->operationType), " with service ID ",
                     operation->serviceId.integer(), " failed with error ", getLastErrorMessage());
        }
    }

    return 0;
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

    auto listenOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::listen,
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

    auto connectOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::connect,
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

void SocketServiceProvider::send(const SocketServiceId serviceId, BytesType message) const
{
    if (serviceId == InvalidServiceId)
    {
        THROW(std::logic_error, "Cannot send message using an InvalidServiceId");
    }

    const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

    auto sendMessageOperation = impl->operations.push({
        .operationType = SocketServiceOperationType::sendMessage,
        .serviceId = serviceId,
        .message = std::move(message),
    });

    const auto numberOfBytesTransferred = 0;
    const auto postResult = ::PostQueuedCompletionStatus(impl->completionPort, numberOfBytesTransferred,
                                                         impl->initialCompletionKey, sendMessageOperation);
    if (postResult == 0)
    {
        THROW(std::runtime_error, "Posting send message operation to SocketServiceProvider failed with error ",
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
