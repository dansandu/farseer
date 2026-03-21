#if defined(__linux__)
#include "dansandu/farseer/internal/linux/linux_socket.hpp"
#include "dansandu/farseer/exception.hpp"

#include <cstring>
#include <sys/socket.h>

using dansandu::farseer::exception::InternalSocketError;

namespace dansandu::farseer::internal::linux::linux_socket
{

namespace
{

constexpr auto invalidSocket = -1;
constexpr auto defaultSocketProtocol = 0;

void closeSocketOrLog(const int socket)
{
    if (socket != invalidSocket && ::close(socket) != 0)
    {
        LOG_CRITICAL("Closing linux socket failed with error ", ::strerror(::errno));
    }
}

}

LinuxSocket::LinuxSocket()
    : socket_{::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, defaultSocketProtocol)}, ipAddress_{}, port_{}
{
    if (socket_ == invalidSocket)
    {
        WTHROW(InternalSocketError, "Creating linux socket failed with error ", ::strerror(::errno));
    }
}

LinuxSocket::LinuxSocket(LinuxSocket&& other) noexcept
    : socket_{std::move(other.socket_)}, ipAddress_{std::move(other.ipAddress_)}, port_{std::move(other.port_)}
{
    other.socket_ = invalidSocket;
    other.ipAddress_.clear();
    other.port_ = 0;
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

        socket_ = std::move(other.socket_);
        ipAddress_ = std::move(other.ipAddress_);
        port_ = std::move(other.port_);

        other.socket_ = invalidSocket;
        other.ipAddress_.clear();
        other.port_ = 0;
    }

    return *this;
}

void LinuxSocket::listen(const std::string& ipAddress, const int port, const int eventPollFileDescriptor)
{
    if (socket_ == invalidSocket)
    {
        WTHROW(InternalSocketError, "Cannot listen on an invalid socket");
    }

    ::sockaddr_in localAddress;
    std::memset(&localAddress, 0, sizeof(localAddress));

    localAddress.sin_family = AF_INET;
    localAddress.sin_port = ::htons(port);

    const auto netResult = ::inet_pton(AF_INET, ipAddress.c_str(), &localAddress.sin_addr.s_addr);
    if (netResult == 0)
    {
        WTHROW(InternalSocketError, "Invalid IP address ", ipAddress);
    }
    else if (netResult < 0)
    {
        WTHROW(InternalSocketError, "inet_pton failed with error ", ::strerror(::errno));
    }

    const auto bindResult = ::bind(socket_, reinterpret_cast<sockaddr*>(&localAddress), sizeof(localAddress));
    if (bindResult != 0)
    {
        WTHROW(InternalSocketError, "Binding to socket failed with error ", ::strerror(::errno));
    }

    const auto maximumListeningQueueSize = 1000;

    const auto listenResult = ::listen(socket_, maximumListeningQueueSize);
    if (listenResult != 0)
    {
        WTHROW(InternalSocketError, "Listening to socket failed with error ", ::strerror(::errno));
    }

    ::epoll_event event;
    std::memset(&event, 0, sizeof(event));

    event.events = EPOLLIN;
    event.data.fd = socket_;

    const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor, EPOLL_CTL_ADD, socket_, &event);
    if (subscribeResult != 0)
    {
        WTHROW(InternalSocketError, "Subscribing listening socket to epoll failed with error ", ::strerror(::errno));
    }

    ipAddress_ = ipAddress;
    port_ = port;
}

}
#endif
