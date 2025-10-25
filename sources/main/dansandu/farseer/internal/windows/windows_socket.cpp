#include "dansandu/farseer/internal/windows/windows_socket.hpp"

using dansandu::farseer::exception::InternalSocketServiceException;
using dansandu::farseer::internal::windows::error::getErrorMessageFromCode;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::error::getLastWsaErrorMessage;

namespace dansandu::farseer::internal::windows::windows_socket
{

static void closeSocketOrLog(SOCKET socket)
{
    if (socket != INVALID_SOCKET && ::closesocket(socket) != 0)
    {
        LOG_CRITICAL("Closing socket failed with error ", getLastWsaErrorMessage());
    }
}

WindowsSocket::WindowsSocket()
    : socket_{INVALID_SOCKET}, acceptFunction_{nullptr}, connectFunction_{nullptr}, ipAddress_{}, port_{}
{
}

WindowsSocket::WindowsSocket(const HANDLE completionPort, const SocketServiceId serviceId)
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
                                                                   serviceId.getInteger(), numberOfConcurrentThreads);
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

WindowsSocket::WindowsSocket(WindowsSocket&& other) noexcept
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

WindowsSocket& WindowsSocket::operator=(WindowsSocket&& other) noexcept
{
    if (this != &other)
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
    }

    return *this;
}

WindowsSocket::~WindowsSocket() noexcept
{
    closeSocketOrLog(socket_);
}

void WindowsSocket::listen(const std::wstring& ipAddress, const int port)
{
    if (socket_ == INVALID_SOCKET)
    {
        WTHROW(InternalSocketServiceException, "Cannot listen on an invalid socket");
    }

    if (connectFunction_ != nullptr)
    {
        WTHROW(InternalSocketServiceException, "Cannot listen on a connection socket");
    }

    if (acceptFunction_ != nullptr)
    {
        WTHROW(InternalSocketServiceException, "Socket is already listening");
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

    const auto maximumListeningQueueSize = 100;

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

    ipAddress_ = ipAddress;
    port_ = port;
}

WindowsSocket WindowsSocket::postAccept(CHAR* const receiveBuffer, const DWORD receiveBufferSize,
                                        const SocketServiceId pendingAcceptServiceId, const HANDLE completionPort,
                                        const LPWSAOVERLAPPED overlapped) const
{
    if (socket_ == INVALID_SOCKET)
    {
        WTHROW(InternalSocketServiceException, "Cannot post accept on an invalid socket");
    }

    if (connectFunction_ != nullptr)
    {
        WTHROW(InternalSocketServiceException, "Cannot post accept on a connection socket");
    }

    if (acceptFunction_ == nullptr)
    {
        WTHROW(InternalSocketServiceException, "Socket must first call listen before accepting connections");
    }

    if (receiveBufferSize < 2 * (sizeof(::sockaddr_in) + 16))
    {
        WTHROW(InternalSocketServiceException,
               "The receive buffer must have enough space to store the local and remote address of the connection");
    }

    auto pendingAcceptSocket = WindowsSocket{completionPort, pendingAcceptServiceId};

    // Force the operation to be asynchronous and do not wait to receive data.
    const auto overrideReceiveBufferSize = 0;

    auto numberOfBytesReceived = DWORD{0};

    const auto acceptResult =
        acceptFunction_(socket_, pendingAcceptSocket.socket_, receiveBuffer, overrideReceiveBufferSize,
                        sizeof(::sockaddr_in) + 16, sizeof(::sockaddr_in) + 16, &numberOfBytesReceived, overlapped);
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

void WindowsSocket::accept(const WindowsSocket& listeningSocket)
{
    if (socket_ == INVALID_SOCKET)
    {
        WTHROW(InternalSocketServiceException, "Cannot accept on an invalid socket");
    }

    if (connectFunction_ != nullptr)
    {
        WTHROW(InternalSocketServiceException, "Cannot accept on a connection socket");
    }

    if (acceptFunction_ != nullptr)
    {
        WTHROW(InternalSocketServiceException, "Cannot accept on a listening socket");
    }

    if (listeningSocket.acceptFunction_ == nullptr)
    {
        WTHROW(InternalSocketServiceException, "Socket passed to accept is not a listening socket");
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

void WindowsSocket::postConnect(const std::wstring& ipAddress, const int port, const LPWSAOVERLAPPED overlapped)
{
    if (socket_ == INVALID_SOCKET)
    {
        WTHROW(InternalSocketServiceException, "Cannot post connect on an invalid socket");
    }

    if (connectFunction_ != nullptr)
    {
        WTHROW(InternalSocketServiceException, "Socket is already connected");
    }

    if (acceptFunction_ != nullptr)
    {
        WTHROW(InternalSocketServiceException, "Cannot post connect on a listening socket");
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

    const auto ioOverlapped = LPWSAOVERLAPPED{nullptr};
    const auto completionRoutine = LPWSAOVERLAPPED_COMPLETION_ROUTINE{nullptr};

    auto connectExGuid = GUID(WSAID_CONNECTEX);
    auto numberOfBytes = DWORD(0);

    const auto ioResult = ::WSAIoctl(socket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &connectExGuid, sizeof(connectExGuid),
                                     static_cast<LPVOID>(&connectFunction_), sizeof(connectFunction_), &numberOfBytes,
                                     ioOverlapped, completionRoutine);
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
                         sendBufferSize, &numberOfBytes, overlapped);
    if (connectResult == FALSE)
    {
        const auto errorCode = ::WSAGetLastError();
        if (errorCode != ERROR_IO_PENDING)
        {
            WTHROW(InternalSocketServiceException, "Connecting socket failed with error ",
                   getErrorMessageFromCode(errorCode));
        }
    }

    ipAddress_ = ipAddress;
    port_ = port;
}

void WindowsSocket::connect()
{
    if (socket_ == INVALID_SOCKET)
    {
        WTHROW(InternalSocketServiceException, "Cannot connect on an invalid socket");
    }

    if (connectFunction_ == nullptr)
    {
        WTHROW(InternalSocketServiceException, "Socket must call postConnect before calling connect");
    }

    if (acceptFunction_ != nullptr)
    {
        WTHROW(InternalSocketServiceException, "Cannot connect on a listening socket");
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

void WindowsSocket::postReceive(CHAR* const receiveBuffer, const ULONG receiveBufferSize,
                                const LPWSAOVERLAPPED overlapped) const
{
    if (socket_ == INVALID_SOCKET)
    {
        WTHROW(InternalSocketServiceException, "Cannot post receive on an invalid socket");
    }

    if (acceptFunction_ != nullptr)
    {
        WTHROW(InternalSocketServiceException, "Cannot post receive on a listening socket");
    }

    auto wsaBuffer = WSABUF{.len = receiveBufferSize, .buf = receiveBuffer};
    auto flags = DWORD{0};

    const auto bufferCount = DWORD{1};
    const auto numberOfBytesReceived = LPDWORD{nullptr};
    const auto completionRoutine = LPWSAOVERLAPPED_COMPLETION_ROUTINE{nullptr};

    // If WSARecv completes immediately the overlapped operation is also scheduled and will be processed in a future
    // call to GetQueuedCompletionStatus.
    const auto receiveResult =
        ::WSARecv(socket_, &wsaBuffer, bufferCount, numberOfBytesReceived, &flags, overlapped, completionRoutine);
    if (receiveResult == SOCKET_ERROR)
    {
        const auto errorCode = ::WSAGetLastError();
        if (errorCode != WSA_IO_PENDING)
        {
            WTHROW(InternalSocketServiceException, "WSARecv failed with error ", getErrorMessageFromCode(errorCode));
        }
    }
}

void WindowsSocket::postSend(CHAR* const bytesToSend, const ULONG numberOfBytesToSend,
                             const LPWSAOVERLAPPED overlapped) const
{
    if (socket_ == INVALID_SOCKET)
    {
        WTHROW(InternalSocketServiceException, "Cannot post send on an invalid socket");
    }

    if (acceptFunction_ != nullptr)
    {
        WTHROW(InternalSocketServiceException, "Cannot post send on a listening socket");
    }

    auto wsaBuffer = WSABUF{.len = numberOfBytesToSend, .buf = bytesToSend};
    auto numberOfBytesSent = DWORD{0};

    const auto bufferCount = DWORD{1};
    const auto flags = DWORD{0};
    const auto completionRoutine = LPWSAOVERLAPPED_COMPLETION_ROUTINE{nullptr};

    const auto sendResult =
        ::WSASend(socket_, &wsaBuffer, bufferCount, &numberOfBytesSent, flags, overlapped, completionRoutine);
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
