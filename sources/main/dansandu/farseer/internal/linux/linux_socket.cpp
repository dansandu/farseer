#if defined(__linux__)
#include "dansandu/farseer/internal/linux/linux_socket.hpp"
#include "dansandu/ballotin/exception.hpp"
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
using dansandu::farseer::internal::linux::error::getLastErrorMessage;

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
        LOG_CRITICAL("Closing linux socket failed with error: ", getLastErrorMessage());
    }
}

}

LinuxSocket::LinuxSocket()
    : socketType_{SocketType::unbound},
      socket_{::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, defaultSocketProtocol)},
      ipAddress_{},
      port_{}
{
    if (socket_ == invalidSocket)
    {
        WTHROW(InternalSocketError, "Creating linux socket failed with error: ", getLastErrorMessage());
    }
}

LinuxSocket LinuxSocket::listen(const std::string& ipAddress, const int port, const int eventPollFileDescriptor)
{
    auto socket = LinuxSocket{};

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
        WTHROW(InternalSocketError, "inet_pton failed with error: ", getLastErrorMessage());
    }

    const auto bindResult =
        ::bind(socket.socket_, reinterpret_cast<const ::sockaddr*>(&localAddress), sizeof(localAddress));
    if (bindResult != 0)
    {
        WTHROW(InternalSocketError, "Binding to socket failed with error: ", getLastErrorMessage());
    }

    const auto maximumListeningQueueSize = 1000;

    const auto listenResult = ::listen(socket.socket_, maximumListeningQueueSize);
    if (listenResult != 0)
    {
        WTHROW(InternalSocketError, "Listening to socket failed with error: ", getLastErrorMessage());
    }

    ::epoll_event event;
    std::memset(&event, 0, sizeof(event));

    event.events = EPOLLIN;
    event.data.fd = socket.socket_;

    const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor, EPOLL_CTL_ADD, socket.socket_, &event);
    if (subscribeResult != 0)
    {
        WTHROW(InternalSocketError, "Subscribing listening socket to epoll failed with error: ", getLastErrorMessage());
    }

    socket.socketType_ = SocketType::listening;
    socket.ipAddress_ = ipAddress;
    socket.port_ = port;

    return socket;
}

LinuxSocket LinuxSocket::connect(const std::string& ipAddress, const int port, const int eventPollFileDescriptor)
{
    auto socket = LinuxSocket{};

    ::sockaddr_in remoteAddress;
    std::memset(&remoteAddress, 0, sizeof(remoteAddress));

    remoteAddress.sin_family = AF_INET;
    remoteAddress.sin_port = ::htons(port);

    const auto netResult = ::inet_pton(AF_INET, ipAddress.c_str(), &remoteAddress.sin_addr.s_addr);
    if (netResult == 0)
    {
        WTHROW(InternalSocketError, "Invalid IP address ", ipAddress);
    }
    else if (netResult < 0)
    {
        WTHROW(InternalSocketError, "inet_pton failed with error: ", getLastErrorMessage());
    }

    const auto connectResult =
        ::connect(socket.socket_, reinterpret_cast<const ::sockaddr*>(&remoteAddress), sizeof(remoteAddress));
    if (connectResult == -1)
    {
        WTHROW(InternalSocketError, "connect failed with error: ", getLastErrorMessage());
    }

    ::epoll_event event;
    std::memset(&event, 0, sizeof(event));

    event.events = EPOLLIN;
    event.data.fd = socket.socket_;

    const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor, EPOLL_CTL_ADD, socket.socket_, &event);
    if (subscribeResult != 0)
    {
        WTHROW(InternalSocketError,
               "Subscribing connection socket to epoll failed with error: ", getLastErrorMessage());
    }

    socket.socketType_ = SocketType::connection;
    socket.ipAddress_ = ipAddress;
    socket.port_ = port;

    return socket;
}

LinuxSocket::LinuxSocket(LinuxSocket&& other) noexcept
    : socketType_{other.socketType_},
      socket_{std::move(other.socket_)},
      ipAddress_{std::move(other.ipAddress_)},
      port_{std::move(other.port_)}
{
    other.socketType_ = SocketType::unbound;
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

        socketType_ = other.socketType_;
        socket_ = other.socket_;
        ipAddress_ = std::move(other.ipAddress_);
        port_ = other.port_;

        other.socketType_ = SocketType::unbound;
        other.socket_ = invalidSocket;
        other.ipAddress_.clear();
        other.port_ = 0;
    }

    return *this;
}

void LinuxSocket::send(const uint8_t* const bytes, const size_t numberOfBytes)
{
    if (socketType_ == SocketType::unbound)
    {
        WTHROW(InternalSocketError, "Cannot send bytes to unbound socket");
    }

    const auto flags = 0;

    const auto sendResult = ::send(socket_, bytes, numberOfBytes, flags);

    if (sendResult == -1)
    {
        WTHROW(InternalSocketError, "Send bytes failed with error: ", getLastErrorMessage());
    }
}

}
#endif
