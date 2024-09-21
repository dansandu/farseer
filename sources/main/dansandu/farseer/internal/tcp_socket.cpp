#include "dansandu/farseer/internal/tcp_socket.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/ballotin/logging.hpp"
#include "dansandu/farseer/internal/error.hpp"
#include "dansandu/farseer/internal/internal_socket_service_exception.hpp"

#include <ioapiset.h>
#include <winsock2.h>
#include <ws2tcpip.h>

using dansandu::ballotin::logging::LogCritical;
using dansandu::farseer::internal::error::getErrorMessageFromCode;
using dansandu::farseer::internal::error::getLastErrorMessage;
using dansandu::farseer::internal::error::getLastWsaErrorMessage;
using dansandu::farseer::internal::internal_socket_service_exception::InternalSocketServiceException;
using dansandu::farseer::internal::socket_service_operation::SocketServiceOperation;
using dansandu::farseer::internal::socket_service_operation::socketServiceOperationBufferSize;

namespace dansandu::farseer::internal::tcp_socket
{

static constexpr auto maximumListeningQueueSize = 100;

static void closeSocketOrLog(SOCKET socket)
{
    if (socket != INVALID_SOCKET && ::closesocket(socket) != 0)
    {
        LogCritical("Closing socket failed with error ", getLastWsaErrorMessage());
    }
}

TcpSocket::TcpSocket(const HANDLE completionPort, const SocketServiceId serviceId)
    : socket_{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)},
      acceptFunction_{nullptr},
      connectFunction_{nullptr},
      ipAddress_{},
      port_{}
{
    if (socket_ != INVALID_SOCKET)
    {
        const auto numberOfConcurrentThreads = 0;
        const auto completionPortResult = ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket_), completionPort,
                                                                   serviceId.integer(), numberOfConcurrentThreads);
        if (completionPortResult == nullptr)
        {
            ::closesocket(socket_);
            WTHROW(InternalSocketServiceException, "CreateIoCompletionPort associate failed with error ",
                   getLastErrorMessage());
        }
    }
    else
    {
        WTHROW(InternalSocketServiceException, "Creating socket failed with error ", getLastWsaErrorMessage());
    }
}

TcpSocket::TcpSocket(TcpSocket&& other) noexcept
    : socket_{std::move(other.socket_)},
      acceptFunction_{std::move(other.acceptFunction_)},
      connectFunction_{std::move(other.connectFunction_)},
      ipAddress_{std::move(other.ipAddress_)},
      port_{std::move(other.port_)}
{
    other.socket_ = INVALID_SOCKET;
    other.acceptFunction_ = nullptr;
    other.connectFunction_ = nullptr;
    other.ipAddress_.clear();
    other.port_ = 0;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept
{
    closeSocketOrLog(socket_);

    socket_ = std::move(other.socket_);
    acceptFunction_ = std::move(other.acceptFunction_);
    connectFunction_ = std::move(other.connectFunction_);
    ipAddress_ = std::move(other.ipAddress_);
    port_ = std::move(other.port_);

    other.socket_ = INVALID_SOCKET;
    other.acceptFunction_ = nullptr;
    other.connectFunction_ = nullptr;
    other.ipAddress_.clear();
    other.port_ = 0;

    return *this;
}

TcpSocket::~TcpSocket() noexcept
{
    closeSocketOrLog(socket_);
}

void TcpSocket::toggleBlocking(const bool block) const
{
    if (socket_ == INVALID_SOCKET)
    {
        THROW(std::logic_error, "Cannot toggle blocking on an invalid socket");
    }

    auto nonBlocking = static_cast<u_long>(!block);
    const auto result = ::ioctlsocket(socket_, FIONBIO, &nonBlocking);
    if (result != 0)
    {
        WTHROW(InternalSocketServiceException, "Toggling socket blocking failed with error ", getLastWsaErrorMessage());
    }
}

void TcpSocket::listen(std::wstring ipAddress, const int port)
{
    if (socket_ == INVALID_SOCKET)
    {
        THROW(std::logic_error, "Cannot listen on an invalid socket");
    }

    if (connectFunction_ != nullptr)
    {
        THROW(std::logic_error, "Cannot listen on a connection socket");
    }

    if (acceptFunction_ != nullptr)
    {
        THROW(std::logic_error, "Socket is already listening");
    }

    auto localAddress = ::sockaddr_in{};
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = ::htons(port);

    const auto netResult = ::InetPton(AF_INET, ipAddress.c_str(), &localAddress.sin_addr.s_addr);
    if (netResult == 0)
    {
        WTHROW(InternalSocketServiceException, "Invalid IP address ", ipAddress);
    }
    else if (netResult < 0)
    {
        WTHROW(InternalSocketServiceException, "InetPton failed with error ", getLastWsaErrorMessage());
    }

    const auto bindResult = ::bind(socket_, reinterpret_cast<SOCKADDR*>(&localAddress), sizeof(localAddress));
    if (bindResult == SOCKET_ERROR)
    {
        WTHROW(InternalSocketServiceException, "Binding to socket failed with error ", getLastWsaErrorMessage());
    }

    const auto listenResult = ::listen(socket_, maximumListeningQueueSize);
    if (listenResult == SOCKET_ERROR)
    {
        WTHROW(InternalSocketServiceException, "Listening to socket failed with error ", getLastWsaErrorMessage());
    }

    const auto overlapped = LPWSAOVERLAPPED{nullptr};
    const auto completionRoutine = LPWSAOVERLAPPED_COMPLETION_ROUTINE{nullptr};

    auto acceptExGuid = GUID(WSAID_ACCEPTEX);
    auto numberOfBytes = DWORD(0);

    const auto ioResult = ::WSAIoctl(socket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptExGuid, sizeof(acceptExGuid),
                                     static_cast<LPVOID>(&acceptFunction_), sizeof(acceptFunction_), &numberOfBytes,
                                     overlapped, completionRoutine);
    if (ioResult == SOCKET_ERROR)
    {
        WTHROW(InternalSocketServiceException, "WSAIoctl failed with error ", getLastWsaErrorMessage());
    }

    ipAddress_ = std::move(ipAddress);
    port_ = std::move(port);
}

TcpSocket TcpSocket::postAccept(const HANDLE completionPort, SocketServiceOperation* operation) const
{
    if (socket_ == INVALID_SOCKET)
    {
        THROW(std::logic_error, "Cannot post accept on an invalid socket");
    }

    if (connectFunction_ != nullptr)
    {
        THROW(std::logic_error, "Cannot post accept on a connection socket");
    }

    if (acceptFunction_ == nullptr)
    {
        THROW(std::logic_error, "Socket must first call listen before accepting connections");
    }

    auto pendingAcceptSocket = TcpSocket{completionPort, operation->serviceId};

    static_assert(socketServiceOperationBufferSize >= 2 * (sizeof(::sockaddr_in) + 16));

    // Initial implementation: receiveDataLength = 0, asynchronous receive with GetQueuedCompletionStatus
    // To consider later: receiveDataLength = socketServiceOperationBufferSize - 2 * (sizeof(sockaddr_in) + 16), can be
    // synchronous
    const auto receiveDataLength = 0;

    auto numberOfBytesReceived = DWORD{0};

    const auto acceptResult =
        acceptFunction_(socket_, pendingAcceptSocket.socket_, operation->buffer, receiveDataLength,
                        sizeof(::sockaddr_in) + 16, sizeof(::sockaddr_in) + 16, &numberOfBytesReceived, operation);
    if (acceptResult == FALSE)
    {
        const auto errorCode = ::WSAGetLastError();
        if (errorCode != ERROR_IO_PENDING)
        {
            WTHROW(InternalSocketServiceException, "Accepting socket failed with error ",
                   getErrorMessageFromCode(errorCode));
        }
    }

    return pendingAcceptSocket;
}

void TcpSocket::accept(const TcpSocket& listeningSocket)
{
    if (socket_ == INVALID_SOCKET)
    {
        THROW(std::logic_error, "Cannot accept on an invalid socket");
    }

    if (connectFunction_ != nullptr)
    {
        THROW(std::logic_error, "Cannot accept on a connection socket");
    }

    if (acceptFunction_ != nullptr)
    {
        THROW(std::logic_error, "Cannot accept on a listening socket");
    }

    if (listeningSocket.acceptFunction_ == nullptr)
    {
        THROW(std::logic_error, "Socket passed to accept is not a listening socket");
    }

    const auto optionalValue = reinterpret_cast<const char*>(&listeningSocket.socket_);
    const auto optionalValueLength = sizeof(listeningSocket.socket_);

    const auto setsockoptResult =
        ::setsockopt(socket_, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, optionalValue, optionalValueLength);
    if (setsockoptResult == SOCKET_ERROR)
    {
        WTHROW(InternalSocketServiceException, "setsockopt failed with error ", getLastWsaErrorMessage());
    }

    auto remoteAddress = ::sockaddr_in{};

    const auto expectedAddressSize = static_cast<int>(sizeof(remoteAddress));

    auto remoteAddressSize = expectedAddressSize;

    const auto getpeernameResult =
        ::getpeername(socket_, reinterpret_cast<SOCKADDR*>(&remoteAddress), &remoteAddressSize);
    if (getpeernameResult == SOCKET_ERROR)
    {
        WTHROW(InternalSocketServiceException, "getpeername failed with error ", getLastWsaErrorMessage());
    }
    else if (remoteAddressSize != expectedAddressSize)
    {
        WTHROW(InternalSocketServiceException, "getpeername truncated the address because the buffer is to small (",
               expectedAddressSize, " bytes were supplied but ", remoteAddressSize, " bytes are needed)");
    }

    wchar_t ipAddressBuffer[INET_ADDRSTRLEN];

    const auto netResult = ::InetNtop(AF_INET, &remoteAddress.sin_addr.s_addr, ipAddressBuffer, INET_ADDRSTRLEN);
    if (netResult == NULL)
    {
        WTHROW(InternalSocketServiceException, "InetNtop failed with error ", getLastWsaErrorMessage());
    }

    ipAddress_ = ipAddressBuffer;

    port_ = ::ntohs(remoteAddress.sin_port);
}

void TcpSocket::postConnect(std::wstring ipAddress, const int port, SocketServiceOperation* operation)
{
    if (socket_ == INVALID_SOCKET)
    {
        THROW(std::logic_error, "Cannot post connect on an invalid socket");
    }

    if (connectFunction_ != nullptr)
    {
        THROW(std::logic_error, "Socket is already connected");
    }

    if (acceptFunction_ != nullptr)
    {
        THROW(std::logic_error, "Cannot post connect on a listening socket");
    }

    auto localAddress = ::sockaddr_in{};
    localAddress.sin_family = AF_INET;
    localAddress.sin_addr.s_addr = ::htonl(INADDR_ANY);
    localAddress.sin_port = ::htons(0);

    const auto bindResult = ::bind(socket_, reinterpret_cast<SOCKADDR*>(&localAddress), sizeof(localAddress));
    if (bindResult == SOCKET_ERROR)
    {
        WTHROW(InternalSocketServiceException, "Binding to socket failed with error ", getLastWsaErrorMessage());
    }

    const auto overlapped = LPWSAOVERLAPPED{nullptr};
    const auto completionRoutine = LPWSAOVERLAPPED_COMPLETION_ROUTINE{nullptr};

    auto connectExGuid = GUID(WSAID_CONNECTEX);
    auto numberOfBytes = DWORD(0);

    const auto ioResult = ::WSAIoctl(socket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &connectExGuid, sizeof(connectExGuid),
                                     static_cast<LPVOID>(&connectFunction_), sizeof(connectFunction_), &numberOfBytes,
                                     overlapped, completionRoutine);
    if (ioResult == SOCKET_ERROR)
    {
        WTHROW(InternalSocketServiceException, "WSAIoctl failed with error ", getLastWsaErrorMessage());
    }

    auto remoteAddress = ::sockaddr_in{};
    remoteAddress.sin_family = AF_INET;
    remoteAddress.sin_port = ::htons(port);

    const auto netResult = ::InetPton(AF_INET, ipAddress.c_str(), &remoteAddress.sin_addr.s_addr);
    if (netResult == 0)
    {
        WTHROW(InternalSocketServiceException, "Invalid IP address ", ipAddress);
    }
    else if (netResult < 0)
    {
        WTHROW(InternalSocketServiceException, "InetPton failed with error ", getLastWsaErrorMessage());
    }

    const auto sendBuffer = PVOID{nullptr};
    const auto sendBufferSize = DWORD{0};

    const auto connectResult =
        connectFunction_(socket_, reinterpret_cast<const SOCKADDR*>(&remoteAddress), sizeof(remoteAddress), sendBuffer,
                         sendBufferSize, &numberOfBytes, operation);
    if (connectResult == FALSE)
    {
        const auto errorCode = ::WSAGetLastError();
        if (errorCode != ERROR_IO_PENDING)
        {
            WTHROW(InternalSocketServiceException, "Connecting socket failed with error ",
                   getErrorMessageFromCode(errorCode));
        }
    }

    ipAddress_ = std::move(ipAddress);
    port_ = std::move(port);
}

void TcpSocket::connect()
{
    if (socket_ == INVALID_SOCKET)
    {
        THROW(std::logic_error, "Cannot connect on an invalid socket");
    }

    if (connectFunction_ == nullptr)
    {
        THROW(std::logic_error, "Socket must call postConnect before calling connect");
    }

    if (acceptFunction_ != nullptr)
    {
        THROW(std::logic_error, "Cannot connect on a listening socket");
    }

    const auto optionalValue = static_cast<const char*>(nullptr);
    const auto optionalValueLength = 0;

    const auto setsockoptResult =
        ::setsockopt(socket_, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, optionalValue, optionalValueLength);
    if (setsockoptResult == SOCKET_ERROR)
    {
        WTHROW(InternalSocketServiceException, "setsockopt failed with error ", getLastWsaErrorMessage());
    }
}

void TcpSocket::postReceive(SocketServiceOperation* operation) const
{
    if (socket_ == INVALID_SOCKET)
    {
        THROW(std::logic_error, "Cannot post receive on an invalid socket");
    }

    if (acceptFunction_ != nullptr)
    {
        THROW(std::logic_error, "Cannot post receive on a listening socket");
    }

    auto buffer = WSABUF{.len = socketServiceOperationBufferSize, .buf = operation->buffer};
    auto numberOfBytesReceived = DWORD{0};
    auto flags = DWORD{0};

    const auto bufferCount = DWORD{1};
    const auto completionRoutine = LPWSAOVERLAPPED_COMPLETION_ROUTINE{nullptr};

    // If WSARecv completes immediately the overlapped operation is also scheduled and will be processed in a future
    // call to GetQueuedCompletionStatus.
    const auto receiveResult =
        ::WSARecv(socket_, &buffer, bufferCount, &numberOfBytesReceived, &flags, operation, completionRoutine);
    if (receiveResult == SOCKET_ERROR)
    {
        const auto errorCode = ::WSAGetLastError();
        if (errorCode != WSA_IO_PENDING)
        {
            WTHROW(InternalSocketServiceException, "WSARecv failed with error ", getErrorMessageFromCode(errorCode));
        }
    }
}

void TcpSocket::postSend(SocketServiceOperation* operation) const
{
    if (socket_ == INVALID_SOCKET)
    {
        THROW(std::logic_error, "Cannot post send on an invalid socket");
    }

    if (acceptFunction_ != nullptr)
    {
        THROW(std::logic_error, "Cannot post send on a listening socket");
    }

    auto buffer = WSABUF{.len = static_cast<ULONG>(operation->message.size()), .buf = operation->message.data()};
    auto numberOfBytesSent = DWORD{0};

    const auto bufferCount = DWORD{1};
    const auto flags = DWORD{0};
    const auto completionRoutine = LPWSAOVERLAPPED_COMPLETION_ROUTINE{nullptr};

    const auto sendResult =
        ::WSASend(socket_, &buffer, bufferCount, &numberOfBytesSent, flags, operation, completionRoutine);
    if (sendResult == SOCKET_ERROR)
    {
        const auto errorCode = ::WSAGetLastError();
        if (errorCode != WSA_IO_PENDING)
        {
            WTHROW(InternalSocketServiceException, "WSASend failed with error ", getErrorMessageFromCode(errorCode));
        }
    }
}

}
