#if defined(__linux__)
#include "dansandu/farseer/internal/linux/linux_socket.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/linux/error.hpp"
#include "dansandu/journey/logging.hpp"

#include <arpa/inet.h>
#include <cstring>
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

constexpr auto invalidSocket = -1;

int createSocket()
{
    const auto result = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

    if (result == invalidSocket)
    {
        WTHROW(InternalSocketError, "Creating linux socket failed with error: ", getLastErrorMessage());
    }

    return result;
}

void closeSocketOrLog(const int socket)
{
    if (socket != invalidSocket && ::close(socket) == -1)
    {
        LOG_ERROR("Closing linux socket failed with error: ", getLastErrorMessage());
    }
}

}

LinuxSocket LinuxSocket::listen(const std::string& ipAddress, const int port, const int eventPollFileDescriptor)
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
        WTHROW(InternalSocketError, "inet_pton failed with error: ", getLastErrorMessage());
    }

    const auto bindResult = ::bind(socket, reinterpret_cast<const ::sockaddr*>(&localAddress), sizeof(localAddress));
    if (bindResult != 0)
    {
        WTHROW(InternalSocketError, "Binding to socket failed with error: ", getLastErrorMessage());
    }

    const auto maximumListeningQueueSize = 1000;

    const auto listenResult = ::listen(socket, maximumListeningQueueSize);
    if (listenResult != 0)
    {
        WTHROW(InternalSocketError, "Listening to socket failed with error: ", getLastErrorMessage());
    }

    ::epoll_event event;

    std::memset(&event, 0, sizeof(event));

    event.events = EPOLLIN | EPOLLET;

    event.data.fd = socket;

    const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor, EPOLL_CTL_ADD, socket, &event);

    if (subscribeResult != 0)
    {
        WTHROW(InternalSocketError, "Subscribing listening socket to epoll failed with error: ", getLastErrorMessage());
    }

    return LinuxSocket{SocketType::listening, socket, eventPollFileDescriptor, ipAddress, port};
}

LinuxSocket LinuxSocket::connect(const std::string& ipAddress, const int port, const int eventPollFileDescriptor)
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
        WTHROW(InternalSocketError, "inet_pton failed with error: ", getLastErrorMessage());
    }

    const auto connectResult =
        ::connect(socket, reinterpret_cast<const ::sockaddr*>(&remoteAddress), sizeof(remoteAddress));
    if (connectResult == -1)
    {
        const auto errorCode = errno;

        if (errorCode != EINPROGRESS)
        {
            WTHROW(InternalSocketError, "connect failed with error: ", getLastErrorMessage());
        }
    }

    ::epoll_event event;

    std::memset(&event, 0, sizeof(event));

    event.events = EPOLLOUT | EPOLLET;

    event.data.fd = socket;

    const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor, EPOLL_CTL_ADD, socket, &event);

    if (subscribeResult != 0)
    {
        WTHROW(InternalSocketError,
               "Subscribing connection socket to epoll failed with error: ", getLastErrorMessage());
    }

    return LinuxSocket{SocketType::connection, socket, eventPollFileDescriptor, ipAddress, port};
}

LinuxSocket::LinuxSocket(SocketType socketType, const int socket, const int eventPollFileDescriptor,
                         const std::string& ipAddress, const int port)
    : socketType_{socketType},
      socket_{socket},
      eventPollFileDescriptor_{eventPollFileDescriptor},
      ipAddress_{ipAddress},
      port_{port}
{
}

LinuxSocket::LinuxSocket(LinuxSocket&& other) noexcept
    : socketType_{other.socketType_},
      socket_{other.socket_},
      eventPollFileDescriptor_{other.eventPollFileDescriptor_},
      ipAddress_{std::move(other.ipAddress_)},
      port_{other.port_}
{
    other.socketType_ = SocketType::unbound;
    other.socket_ = invalidSocket;
    other.eventPollFileDescriptor_ = invalidSocket;
    other.ipAddress_.clear();
    other.port_ = 0;
}

LinuxSocket::~LinuxSocket() noexcept
{
    if (socket_ == invalidSocket)
    {
        return;
    }

    const auto event = nullptr;

    const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor_, EPOLL_CTL_DEL, socket_, event);

    if (subscribeResult == -1)
    {
        LOG_ERROR("Unsubscribing socket from event poll failed with error: ", getLastErrorMessage());
    }

    closeSocketOrLog(socket_);
}

LinuxSocket& LinuxSocket::operator=(LinuxSocket&& other) noexcept
{
    if (this != &other)
    {
        closeSocketOrLog(socket_);

        socketType_ = other.socketType_;
        socket_ = other.socket_;
        eventPollFileDescriptor_ = other.eventPollFileDescriptor_;
        ipAddress_ = std::move(other.ipAddress_);
        port_ = other.port_;

        other.socketType_ = SocketType::unbound;
        other.socket_ = invalidSocket;
        other.eventPollFileDescriptor_ = invalidSocket;
        other.ipAddress_.clear();
        other.port_ = 0;
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

    const auto socket = ::accept(socket_, reinterpret_cast<::sockaddr*>(&remoteAddress), &remoteAddressSize);

    if (socket == invalidSocket)
    {
        const auto errorCode = errno;

        if (errorCode == EAGAIN || errorCode == EWOULDBLOCK)
        {
            return {};
        }

        WTHROW(InternalSocketError, "Accepting connection failed with error: ", getErrorMessage(errorCode));
    }
    else
    {
        SCOPE_FAILURE([&]() { closeSocketOrLog(socket); });

        if (remoteAddressSize != sizeof(remoteAddress))
        {
            WTHROW(InternalSocketError, "Accepting connection failed because the address wouldn't fit the buffer");
        }

        char ipAddressBuffer[INET_ADDRSTRLEN];

        const auto netResult = ::inet_ntop(AF_INET, &remoteAddress.sin_addr, ipAddressBuffer, INET_ADDRSTRLEN);

        if (netResult == nullptr)
        {
            WTHROW(InternalSocketError, "inet_ntop failed with error ", getLastErrorMessage());
        }

        const auto port = ::ntohs(remoteAddress.sin_port);

        ::epoll_event event;

        std::memset(&event, 0, sizeof(event));

        event.events = EPOLLOUT | EPOLLET;

        event.data.fd = socket;

        const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor_, EPOLL_CTL_ADD, socket, &event);

        if (subscribeResult != 0)
        {
            WTHROW(InternalSocketError,
                   "Subscribing accepted socket to epoll failed with error: ", getLastErrorMessage());
        }

        return LinuxSocket{SocketType::accepted, socket, eventPollFileDescriptor_, ipAddressBuffer, port};
    }
}

void LinuxSocket::send(const uint8_t* const bytes, const size_t numberOfBytes)
{
    if (socketType_ == SocketType::unbound)
    {
        WTHROW(InternalSocketError, "Cannot send bytes to unbound socket");
    }

    if (socketType_ == SocketType::listening)
    {
        WTHROW(InternalSocketError, "Cannot send bytes to listening socket");
    }

    const auto flags = 0;

    const auto sendResult = ::send(socket_, bytes, numberOfBytes, flags);

    if (sendResult == -1)
    {
        WTHROW(InternalSocketError, "Send bytes failed with error: ", getLastErrorMessage());
    }
}

std::vector<uint8_t> LinuxSocket::receive()
{
    if (socketType_ == SocketType::unbound)
    {
        WTHROW(InternalSocketError, "Cannot receive bytes from unbound socket");
    }

    if (socketType_ == SocketType::listening)
    {
        WTHROW(InternalSocketError, "Cannot receive bytes from listening socket");
    }

    auto result = std::vector<uint8_t>{};

    constexpr auto bufferSize = 4096uz;

    uint8_t buffer[bufferSize];

    const auto flags = 0;

    while (true)
    {
        const auto receiveResult = ::recv(socket_, buffer, bufferSize, flags);

        if (receiveResult == -1)
        {
            const auto errorCode = errno;

            if (errorCode == EAGAIN || errorCode == EWOULDBLOCK)
            {
                break;
            }

            WTHROW(InternalSocketError, "Receive bytes failed with error: ", getErrorMessage(errorCode));
        }
        else
        {
            result.insert(result.end(), buffer, buffer + receiveResult);
        }
    }

    return result;
}

}
#endif
