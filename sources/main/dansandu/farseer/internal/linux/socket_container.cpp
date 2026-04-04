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
using dansandu::farseer::internal::linux::event_poll::EventPoll;
using dansandu::farseer::internal::linux::linux_socket::LinuxSocket;
using dansandu::farseer::internal::linux::linux_socket::SocketType;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::journey::exception::WideException;

namespace dansandu::farseer::internal::linux::socket_container
{

SocketContainer::SocketContainer(EventPoll& eventPoll) : eventPoll_{eventPoll}
{
}

SocketContainer::~SocketContainer() noexcept
{
    for (const auto& entry : sockets_)
    {
        eventPoll_.unsubscribe(entry.second.socket.getSocketFileDescriptor());
    }
}

Socket& SocketContainer::insertSocket(const uint32_t events, Socket&& socket)
{
    const auto socketIdentifier = socket.socketIdentifier;

    const auto [socketPosition, socketInserted] = sockets_.emplace(socketIdentifier, std::move(socket));

    if (!socketInserted)
    {
        THROW(std::logic_error, "Couldn't insert socket with ID ", socketIdentifier.getUnderlying(),
              " because its ID is used by another socket");
    }

    SCOPE_FAILURE([&] { sockets_.erase(socketPosition); });

    const auto socketFileDescriptor = socketPosition->second.socket.getSocketFileDescriptor();

    const auto [descriptorPosition, descriptorInserted] =
        fileDescriptorsToSockets_.emplace(socketFileDescriptor, &socketPosition->second);

    if (!descriptorInserted)
    {
        THROW(std::logic_error, "Couldn't insert socket with ID ", socketIdentifier.getUnderlying(),
              " because its file descriptor is used by another socket");
    }

    SCOPE_FAILURE([&] { fileDescriptorsToSockets_.erase(descriptorPosition); });

    eventPoll_.subscribe(socketFileDescriptor, events);

    return socketPosition->second;
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
    auto& socket = insertSocket(
        EPOLLIN | EPOLLET,
        Socket{
            .socketIdentifier = socketIdentifier,
            .listeningSocketIdentifier = invalidSocketIdentifier,
            .socket = LinuxSocket::listen(ipAddress, port),
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
    insertSocket(EPOLLOUT | EPOLLET,
                 Socket{
                     .socketIdentifier = socketIdentifier,
                     .listeningSocketIdentifier = invalidSocketIdentifier,
                     .socket = LinuxSocket::connect(ipAddress, port),
                     .protocolReader = ProtocolReader{[&](const SocketIdentifier receivingSocketIdentifier,
                                                          std::vector<uint8_t>&& response)
                                                      { sendBytes(receivingSocketIdentifier, response); }},
                     .connectionCallback = std::move(connectionCallback),
                 });
}

void SocketContainer::sendBytes(const SocketIdentifier socketIdentifier, const std::span<uint8_t> bytes)
{
    auto& socket = getSocketOrThrow(socketIdentifier);

    LOG_DEBUG("Sending ", bytes.size(), " bytes to socket with ID ", socketIdentifier.getUnderlying());

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
                const auto socketFileDescriptor = position->second.socket.getSocketFileDescriptor();

                eventPoll_.unsubscribe(socketFileDescriptor);

                fileDescriptorsToSockets_.erase(socketFileDescriptor);

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
            LOG_ERROR("Error trying to close socket with ID ", socketIdentifier.getUnderlying(), ": ",
                      wideException.getMessage());
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR("Error trying to close socket with ID ", socketIdentifier.getUnderlying(), ": ",
                      exception.what());
        }
    }
}

void SocketContainer::handleSocketEventWork(Socket& socket, const uint32_t socketEvents,
                                            Sequencer<SocketIdentifier>& socketIdentifierSequencer)
{
    if (socketEvents & EPOLLRDHUP)
    {
        LOG_INFO("Connection was closed with socket with ID ", socket.socketIdentifier.getUnderlying(), " and address ",
                 socket.socket.getIpAddress(), ':', socket.socket.getPort());

        eraseSocket(socket.socketIdentifier);

        return;
    }

    if (socketEvents & EPOLLERR)
    {
        LOG_WARNING("Connection was aborted with socket with ID ", socket.socketIdentifier.getUnderlying(),
                    " and address ", socket.socket.getIpAddress(), ':', socket.socket.getPort());

        eraseSocket(socket.socketIdentifier);

        return;
    }

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

            auto& acceptedSocket =
                insertSocket(EPOLLOUT | EPOLLET,
                             Socket{
                                 .socketIdentifier = acceptedSocketIdentifier,
                                 .listeningSocketIdentifier = socket.socketIdentifier,
                                 .socket = std::move(*candidateSocket),
                                 .protocolReader = ProtocolReader{[&](const SocketIdentifier receivingSocketIdentifier,
                                                                      std::vector<uint8_t>&& response)
                                                                  { sendBytes(receivingSocketIdentifier, response); }},
                                 .connectionCallback = {},
                             });

            socket.connectionCallback(SocketEvent::clientOpen, acceptedSocketIdentifier);

            LOG_INFO("Accepted client socket with ID ", acceptedSocketIdentifier.getUnderlying(), " and address ",
                     acceptedSocket.socket.getIpAddress(), ":", acceptedSocket.socket.getPort());
        }
    }
    else if (socket.socket.getSocketType() == SocketType::connection)
    {
        if (socketEvents & EPOLLOUT)
        {
            eventPoll_.modify(socket.socket.getSocketFileDescriptor(), EPOLLIN | EPOLLET);

            socket.connectionCallback(SocketEvent::clientOpen, invalidSocketIdentifier);

            LOG_INFO("Connected to socket with ID ", socket.socketIdentifier.getUnderlying(), " and address ",
                     socket.socket.getIpAddress(), ":", socket.socket.getPort());
        }
        else if (socketEvents & EPOLLIN)
        {
            const auto receivedBytes = socket.socket.receiveBytes();

            LOG_INFO("Received bytes ", receivedBytes.size(), " from socket with ID ",
                     socket.socketIdentifier.getUnderlying(), " and address ", socket.socket.getIpAddress(), ":",
                     socket.socket.getPort());

            socket.protocolReader.read(socket.socketIdentifier, receivedBytes);
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

        eventPoll_.unsubscribe(socketFileDescriptor);

        return;
    }

    const auto socketIdentifier = socketPosition->second->socketIdentifier;

    LOG_DEBUG("Processing events for socket with ID ", socketIdentifier.getUnderlying());

    try
    {
        handleSocketEventWork(*(socketPosition->second), socketEvents, socketIdentifierSequencer);
    }
    catch (const WideException& exception)
    {
        LOG_ERROR("Processing events for socket with ID ", socketIdentifier.getUnderlying(),
                  " failed with wide exception: ", exception.getMessage());

        eraseSocket(socketIdentifier);
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR("Processing events for socket with ID ", socketIdentifier.getUnderlying(),
                  " failed with exception: ", exception.what());

        eraseSocket(socketIdentifier);
    }
}

}
#endif
