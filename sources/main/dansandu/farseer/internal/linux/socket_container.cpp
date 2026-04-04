#if defined(__linux__)
#include "dansandu/farseer/internal/linux/socket_container.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/linux/error.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/journey/logging.hpp"

#include <sys/epoll.h>

using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::linux::error::getLastErrorMessage;
using dansandu::farseer::internal::linux::linux_socket::LinuxSocket;
using dansandu::farseer::internal::linux::linux_socket::SocketType;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::journey::exception::WideException;

namespace dansandu::farseer::internal::linux::socket_container
{

SocketContainer::SocketContainer(const int eventPollFileDescriptor) : eventPollFileDescriptor_{eventPollFileDescriptor}
{
}

SocketContainer::~SocketContainer() noexcept
{
}

Socket& SocketContainer::insertSocket(Socket&& socket)
{
    const auto socketIdentifier = socket.socketIdentifier;

    const auto [position, inserted] = sockets_.emplace(socketIdentifier, std::move(socket));

    if (!inserted)
    {
        THROW(std::logic_error, "Couldn't insert socket with ID ", socketIdentifier.getUnderlying(),
              " because the ID is used by another socket");
    }

    SCOPE_FAILURE([&] { sockets_.erase(position); });

    const auto [descriptorPosition, descriptorInserted] =
        fileDescriptorsToSockets_.emplace(position->second.socket.getSocketFileDescriptor(), &position->second);

    if (!descriptorInserted)
    {
        THROW(std::logic_error, "Couldn't insert socket with ID ", socketIdentifier.getUnderlying(),
              " because its file descriptor is used by another socket");
    }

    return position->second;
}

Socket& SocketContainer::getSocketOrThrow(const SocketIdentifier socketIdentifier)
{
    const auto position = sockets_.find(socketIdentifier);

    if (position != sockets_.end())
    {
        return position->second;
    }

    WTHROW(InternalSocketError, "Couldn't find socket with ID ", socketIdentifier.getUnderlying());
}

void SocketContainer::listen(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                             ConnectionCallback&& connectionCallback)
{
    auto& socket = insertSocket(Socket{
        .socketIdentifier = socketIdentifier,
        .listeningSocketIdentifier = invalidSocketIdentifier,
        .socket = LinuxSocket::listen(ipAddress, port, eventPollFileDescriptor_),
        .protocolReader =
            ProtocolReader{[&](const SocketIdentifier receivingSocketIdentifier, std::vector<uint8_t>&& response)
                           { sendBytes(receivingSocketIdentifier, response); }},
        .connectionCallback = std::move(connectionCallback),
    });

    socket.connectionCallback(SocketEvent::serverOpen, socket.socketIdentifier);

    LOG_INFO("Opened listening socket with ID ", socket.socketIdentifier.getUnderlying(), " and address ",
             socket.socket.getIpAddress(), ":", socket.socket.getPort());
}

void SocketContainer::connect(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                              ConnectionCallback&& connectionCallback)
{
    insertSocket(Socket{
        .socketIdentifier = socketIdentifier,
        .listeningSocketIdentifier = invalidSocketIdentifier,
        .socket = LinuxSocket::connect(ipAddress, port, eventPollFileDescriptor_),
        .protocolReader =
            ProtocolReader{[&](const SocketIdentifier receivingSocketIdentifier, std::vector<uint8_t>&& response)
                           { sendBytes(receivingSocketIdentifier, response); }},
        .connectionCallback = std::move(connectionCallback),
    });
}

void SocketContainer::sendBytes(const SocketIdentifier socketIdentifier, const std::span<uint8_t> bytes)
{
    auto& socket = getSocketOrThrow(socketIdentifier);

    socket.socket.sendBytes(bytes);
}

void SocketContainer::eraseSocket(const SocketIdentifier socketIdentifier)
{
    const auto position = sockets_.find(socketIdentifier);

    if (position != sockets_.end())
    {
        SCOPE_EXIT(
            [&]()
            {
                const auto fileDescriptor = position->second.socket.getSocketFileDescriptor();

                fileDescriptorsToSockets_.erase(fileDescriptor);

                sockets_.erase(position);
            });

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

            LOG_INFO("Socket with ID ", socketIdentifier.getUnderlying(), " and address ", socket.socket.getIpAddress(),
                     ":", socket.socket.getPort(), " was closed");
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
    }
}

void SocketContainer::handleSocketEventWork(Socket& socket, const uint32_t socketEvents,
                                            Sequencer<SocketIdentifier>& socketIdentifierSequencer)
{
    if (socket.socket.getSocketType() == SocketType::listening)
    {
        while (true)
        {
            auto candidateSocket = socket.socket.accept();

            if (!candidateSocket)
            {
                break;
            }

            const auto acceptedSocketIdentifier = socketIdentifierSequencer.generate();

            auto& acceptedSocket = insertSocket(Socket{
                .socketIdentifier = acceptedSocketIdentifier,
                .listeningSocketIdentifier = socket.socketIdentifier,
                .socket = std::move(*candidateSocket),
                .protocolReader = ProtocolReader{[&](const SocketIdentifier receivingSocketIdentifier,
                                                     std::vector<uint8_t>&& response)
                                                 { sendBytes(receivingSocketIdentifier, response); }},
                .connectionCallback = {},
            });

            LOG_INFO("Accepted client socket ID ", acceptedSocketIdentifier.getUnderlying(), " and address ",
                     acceptedSocket.socket.getIpAddress(), ':', acceptedSocket.socket.getPort());
        }
    }
    else
    {
        if (socketEvents & EPOLLOUT)
        {
            LOG_INFO("Connected to socket ID ", socket.socketIdentifier.getUnderlying(), " and address ",
                     socket.socket.getIpAddress(), ':', socket.socket.getPort());
        }

        if (socketEvents & EPOLLIN)
        {
            LOG_INFO("Received bytes from socket ID ", socket.socketIdentifier.getUnderlying(), " and address ",
                     socket.socket.getIpAddress(), ':', socket.socket.getPort());

            socket.socket.receiveBytes();
        }

        if (socketEvents & EPOLLRDHUP)
        {
            LOG_INFO("Connection closed with socket ID ", socket.socketIdentifier.getUnderlying(), " and address ",
                     socket.socket.getIpAddress(), ':', socket.socket.getPort());
        }

        if (socketEvents & EPOLLERR)
        {
            LOG_INFO("Connection aborted with socket ID ", socket.socketIdentifier.getUnderlying(), " and address ",
                     socket.socket.getIpAddress(), ':', socket.socket.getPort());
        }
    }
}

void SocketContainer::handleSocketEvent(const int socketFileDescriptor, const uint32_t socketEvents,
                                        Sequencer<SocketIdentifier>& socketIdentifierSequencer)
{
    const auto socketPosition = fileDescriptorsToSockets_.find(socketFileDescriptor);

    if (socketPosition == fileDescriptorsToSockets_.end())
    {
        LOG_ERROR("Unsubscribing unused socket");

        const auto event = nullptr;

        const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor_, EPOLL_CTL_DEL, socketFileDescriptor, event);

        if (subscribeResult == -1)
        {
            LOG_ERROR("Error unsubscribing unused socket from event poll: ", getLastErrorMessage());
        }

        return;
    }

    const auto socketIdentifier = socketPosition->second->socketIdentifier.getUnderlying();

    LOG_DEBUG("Processing events for socket with ID ", socketIdentifier);

    try
    {
        handleSocketEventWork(*(socketPosition->second), socketEvents, socketIdentifierSequencer);
    }
    catch (const WideException& exception)
    {
        LOG_ERROR("Processing events for socket with ID ", socketIdentifier,
                  " failed with wide exception: ", exception.getMessage());
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR("Processing events for socket with ID ", socketIdentifier,
                  " failed with exception: ", exception.what());
    }
}

}
#endif
