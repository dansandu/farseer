#if defined(__linux__)
#include "dansandu/farseer/internal/linux/linux_socket.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/linux/error.hpp"
#include "dansandu/journey/logging.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::linux::error::getErrorMessage;
using dansandu::farseer::internal::linux::error::getLastErrorMessage;

namespace dansandu::farseer::internal::linux::linux_socket
{

namespace
{

constexpr auto invalidFileDescriptor = -1;

int createSocket()
{
    const auto result = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);

    if (result == invalidFileDescriptor)
    {
        WTHROW(InternalSocketError, "Error creating socket: ", getLastErrorMessage());
    }

    return result;
}

void closeSocketOrLog(const int socket)
{
    if (socket != invalidFileDescriptor && ::close(socket) == -1)
    {
        LOG_ERROR("Error closing socket: ", getLastErrorMessage());
    }
}

}

LinuxSocket LinuxSocket::listen(const std::string& ipAddress, const int port)
{
    const auto socket = createSocket();

    SCOPE_FAILURE([&]() { closeSocketOrLog(socket); });

    ::sockaddr_in localAddress;

    std::memset(&localAddress, 0, sizeof(localAddress));

    localAddress.sin_family = AF_INET;

    localAddress.sin_port = ::htons(port);

    const auto netResult = ::inet_pton(AF_INET, ipAddress.c_str(), &localAddress.sin_addr);

    if (netResult == 0)
    {
        WTHROW(InternalSocketError, "Invalid IP address ", ipAddress);
    }
    else if (netResult < 0)
    {
        WTHROW(InternalSocketError, "Error converting IP address: ", getLastErrorMessage());
    }

    const auto bindResult = ::bind(socket, reinterpret_cast<const ::sockaddr*>(&localAddress), sizeof(localAddress));

    if (bindResult != 0)
    {
        WTHROW(InternalSocketError, "Error binding socket: ", getLastErrorMessage());
    }

    const auto maximumListeningQueueSize = 1000;

    const auto listenResult = ::listen(socket, maximumListeningQueueSize);

    if (listenResult != 0)
    {
        WTHROW(InternalSocketError, "Error listening to socket: ", getLastErrorMessage());
    }

    return LinuxSocket{ipAddress, port, socket, SocketType::listening};
}

LinuxSocket LinuxSocket::connect(const std::string& ipAddress, const int port)
{
    const auto socket = createSocket();

    SCOPE_FAILURE([&]() { closeSocketOrLog(socket); });

    ::sockaddr_in remoteAddress;

    std::memset(&remoteAddress, 0, sizeof(remoteAddress));

    remoteAddress.sin_family = AF_INET;

    remoteAddress.sin_port = ::htons(port);

    const auto netResult = ::inet_pton(AF_INET, ipAddress.c_str(), &remoteAddress.sin_addr);

    if (netResult == 0)
    {
        WTHROW(InternalSocketError, "Invalid IP address ", ipAddress);
    }
    else if (netResult < 0)
    {
        WTHROW(InternalSocketError, "Error converting IP address: ", getLastErrorMessage());
    }

    const auto connectResult =
        ::connect(socket, reinterpret_cast<const ::sockaddr*>(&remoteAddress), sizeof(remoteAddress));

    if (connectResult == -1)
    {
        const auto errorCode = errno;

        if (errorCode != EINPROGRESS)
        {
            WTHROW(InternalSocketError, "Error connecting to socket: ", getLastErrorMessage());
        }
    }

    return LinuxSocket{ipAddress, port, socket, SocketType::connecting};
}

LinuxSocket::LinuxSocket(const std::string& ipAddress, const int port, const int socket, const SocketType socketType)
    : outgoingBuffer_{}, ipAddress_{ipAddress}, port_{port}, socket_{socket}, socketType_{socketType}
{
}

LinuxSocket::LinuxSocket(LinuxSocket&& other) noexcept
    : outgoingBuffer_{std::move(other.outgoingBuffer_)},
      ipAddress_{std::move(other.ipAddress_)},
      port_{other.port_},
      socket_{other.socket_},
      socketType_{other.socketType_}
{
    other.outgoingBuffer_.clear();
    other.ipAddress_.clear();
    other.port_ = 0;
    other.socket_ = invalidFileDescriptor;
    other.socketType_ = SocketType::unbound;
}

LinuxSocket::~LinuxSocket() noexcept
{
    closeSocketOrLog(socket_);
}

LinuxSocket& LinuxSocket::operator=(LinuxSocket&& other) noexcept
{
    if (this != &other)
    {
        closeSocketOrLog(socket_);

        outgoingBuffer_ = std::move(other.outgoingBuffer_);
        ipAddress_ = std::move(other.ipAddress_);
        port_ = other.port_;
        socket_ = other.socket_;
        socketType_ = other.socketType_;

        other.outgoingBuffer_.clear();
        other.ipAddress_.clear();
        other.port_ = 0;
        other.socket_ = invalidFileDescriptor;
        other.socketType_ = SocketType::unbound;
    }

    return *this;
}

std::optional<LinuxSocket> LinuxSocket::accept()
{
    if (socketType_ != SocketType::listening)
    {
        WTHROW(InternalSocketError, "Can only accept connections from a listening socket");
    }

    ::sockaddr_in remoteAddress;

    ::socklen_t remoteAddressSize = sizeof(remoteAddress);

    std::memset(&remoteAddress, 0, sizeof(remoteAddress));

    const auto flags = SOCK_NONBLOCK | SOCK_CLOEXEC;

    const auto acceptedSocket =
        ::accept4(socket_, reinterpret_cast<::sockaddr*>(&remoteAddress), &remoteAddressSize, flags);

    if (acceptedSocket == invalidFileDescriptor)
    {
        const auto errorCode = errno;

        if (errorCode == EAGAIN || errorCode == EWOULDBLOCK)
        {
            return {};
        }

        WTHROW(InternalSocketError, "Error accepting socket: ", getErrorMessage(errorCode));
    }
    else
    {
        SCOPE_FAILURE([&]() { closeSocketOrLog(acceptedSocket); });

        if (remoteAddressSize != sizeof(remoteAddress))
        {
            WTHROW(InternalSocketError, "Accept failed because the address wouldn't fit the buffer");
        }

        char ipAddressBuffer[INET_ADDRSTRLEN];

        const auto netResult = ::inet_ntop(AF_INET, &remoteAddress.sin_addr, ipAddressBuffer, INET_ADDRSTRLEN);

        if (netResult == nullptr)
        {
            WTHROW(InternalSocketError, "Error converting IP address: ", getLastErrorMessage());
        }

        const auto port = ::ntohs(remoteAddress.sin_port);

        return LinuxSocket{ipAddressBuffer, port, acceptedSocket, SocketType::accepted};
    }
}

void LinuxSocket::connected()
{
    socketType_ = SocketType::connected;
}

bool LinuxSocket::sendBytes(const std::span<const uint8_t> bytes)
{
    if (socketType_ != SocketType::accepted && socketType_ != SocketType::connected)
    {
        WTHROW(InternalSocketError, "Can only send bytes to an accepted or connected socket");
    }

    outgoingBuffer_.insert(outgoingBuffer_.end(), bytes.begin(), bytes.end());

    if (outgoingBuffer_.empty())
    {
        return true;
    }

    const auto flags = 0;

    const auto numberOfBytesSent = ::send(socket_, outgoingBuffer_.data(), outgoingBuffer_.size(), flags);

    if (numberOfBytesSent == -1)
    {
        const auto errorCode = errno;

        if (errorCode == EAGAIN || errorCode == EWOULDBLOCK)
        {
            return false;
        }

        WTHROW(InternalSocketError, "Error sending bytes to socket: ", getErrorMessage(errorCode));
    }

    const auto exhausted = numberOfBytesSent == static_cast<ssize_t>(outgoingBuffer_.size());

    outgoingBuffer_.erase(outgoingBuffer_.begin(), outgoingBuffer_.begin() + numberOfBytesSent);

    return exhausted;
}

std::pair<std::vector<uint8_t>, bool> LinuxSocket::receiveBytes()
{
    if (socketType_ != SocketType::accepted && socketType_ != SocketType::connected)
    {
        WTHROW(InternalSocketError, "Can only receive bytes from an accepted or connected socket");
    }

    auto result = std::pair<std::vector<uint8_t>, bool>{};

    result.second = false;

    constexpr auto maximumBufferSize = 4096uz;

    uint8_t buffer[maximumBufferSize];

    const auto flags = 0;

    while (true)
    {
        const auto numberOfBytesReceived = ::recv(socket_, buffer, maximumBufferSize, flags);

        if (numberOfBytesReceived == 0)
        {
            result.second = true;
            return result;
        }
        else if (numberOfBytesReceived == -1)
        {
            const auto errorCode = errno;

            if (errorCode == EAGAIN || errorCode == EWOULDBLOCK)
            {
                break;
            }

            WTHROW(InternalSocketError, "Error receiving bytes from socket: ", getErrorMessage(errorCode));
        }
        else
        {
            result.first.insert(result.first.end(), buffer, buffer + numberOfBytesReceived);
        }
    }

    return result;
}

}
#endif
