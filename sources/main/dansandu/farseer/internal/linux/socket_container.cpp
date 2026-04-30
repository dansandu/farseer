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

SocketContainer::SocketContainer(EventPoll& eventPoll, Sequencer<SocketIdentifier>& socketIdentifierSequencer)
    : eventPoll_{eventPoll}, socketIdentifierSequencer_{socketIdentifierSequencer}
{
}

SocketContainer::~SocketContainer() noexcept
{
    for (const auto& entry : sockets_)
    {
        eventPoll_.unsubscribe(entry.second.socket.getSocketFileDescriptor());
    }
}

size_t SocketContainer::getNumberOfSockets() const
{
    return sockets_.size();
}

Socket& SocketContainer::insertSocket(const uint32_t events, Socket&& socket)
{
    const auto socketIdentifier = socket.socketIdentifier;

    const auto [socketPosition, socketInserted] = sockets_.emplace(socketIdentifier, std::move(socket));

    if (!socketInserted)
    {
        THROW(std::logic_error, "Couldn't insert socket with ID ", socketIdentifier,
              " because its ID is used by another socket");
    }

    SCOPE_FAILURE([&] { sockets_.erase(socketPosition); });

    const auto socketFileDescriptor = socketPosition->second.socket.getSocketFileDescriptor();

    const auto [descriptorPosition, descriptorInserted] =
        fileDescriptorsToSockets_.emplace(socketFileDescriptor, &socketPosition->second);

    if (!descriptorInserted)
    {
        THROW(std::logic_error, "Couldn't insert socket with ID ", socketIdentifier,
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

    WTHROW(InternalSocketError, "Couldn't find socket with ID ", socketIdentifier);
}

void SocketContainer::listen(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                             UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback)
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

    LOG_INFO("Opened listening socket with ID ", socket.socketIdentifier, " and address ", socket.socket.getIpAddress(),
             ":", socket.socket.getPort());
}

void SocketContainer::connect(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                              UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback)
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

void SocketContainer::registerMessageConsumer(const SocketIdentifier socketIdentifier,
                                              const ProtocolIdentifier protocolIdentifier,
                                              UniqueFunction<void(std::any&&)>&& messageConsumer)

{
    auto& socket = getSocketOrThrow(socketIdentifier);

    socket.protocolReader.registerMessageConsumer(protocolIdentifier, std::move(messageConsumer));

    LOG_INFO("Registered message consumer with protocol ID ", protocolIdentifier, " and socket socket ID ",
             socketIdentifier);
}

void SocketContainer::registerRequestCallback(const SocketIdentifier socketIdentifier,
                                              const ProtocolIdentifier protocolIdentifier,
                                              UniqueFunction<std::any(std::any&&)>&& requestConsumer)
{
    auto& socket = getSocketOrThrow(socketIdentifier);

    socket.protocolReader.registerRequestConsumer(protocolIdentifier, std::move(requestConsumer));

    LOG_INFO("Registered request consumer with protocol ID ", protocolIdentifier, " and socket ID ", socketIdentifier);
}

void SocketContainer::sendBytes(Socket& socket, const std::span<const uint8_t> bytes)
{
    LOG_DEBUG("Sending ", bytes.size(), " bytes to socket with ID ", socket.socketIdentifier);

    const auto exhausted = socket.socket.sendBytes(bytes);

    if (exhausted)
    {
        eventPoll_.setEvents(socket.socket.getSocketFileDescriptor(), EPOLLIN | EPOLLET);
    }
    else
    {
        LOG_DEBUG("Bytes sent to socket with ID ", socket.socketIdentifier, " were not exhausted");

        eventPoll_.setEvents(socket.socket.getSocketFileDescriptor(), EPOLLIN | EPOLLOUT | EPOLLET);
    }
}

void SocketContainer::sendBytes(const SocketIdentifier socketIdentifier, const std::span<const uint8_t> bytes)
{
    auto& socket = getSocketOrThrow(socketIdentifier);

    sendBytes(socket, bytes);
}

void SocketContainer::sendRequest(const SocketIdentifier socketIdentifier,
                                  const ProtocolSequenceNumber protocolSequenceNumber,
                                  const std::span<const uint8_t> bytes,
                                  UniqueFunction<void(std::any&&)>&& responseConsumer)
{
    auto& socket = getSocketOrThrow(socketIdentifier);

    socket.protocolReader.registerOneShotResponseConsumer(protocolSequenceNumber, std::move(responseConsumer));

    sendBytes(socket, bytes);
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

            LOG_INFO("Socket with ID ", socketIdentifier, " and address ", socket.socket.getIpAddress(), ":",
                     socket.socket.getPort(), " was closed");
        }
        catch (const WideException& wideException)
        {
            LOG_ERROR("Error trying to close socket with ID ", socketIdentifier, ": ", wideException.getMessage());
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR("Error trying to close socket with ID ", socketIdentifier, ": ", exception.what());
        }
    }
}

void SocketContainer::handleListeningSocketEvents(Socket& socket)
{
    while (true)
    {
        auto candidateSocket = socket.socket.accept();

        if (!candidateSocket)
        {
            break;
        }

        const auto acceptedSocketIdentifier = socketIdentifierSequencer_.generate();

        auto& acceptedSocket =
            insertSocket(EPOLLIN | EPOLLET,
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

        LOG_INFO("Accepted client socket with ID ", acceptedSocketIdentifier, " and address ",
                 acceptedSocket.socket.getIpAddress(), ":", acceptedSocket.socket.getPort());
    }
}

void SocketContainer::handleConnectingSocketEvents(Socket& socket)
{
    eventPoll_.setEvents(socket.socket.getSocketFileDescriptor(), EPOLLIN | EPOLLET);

    socket.socket.connected();

    socket.connectionCallback(SocketEvent::clientOpen, invalidSocketIdentifier);

    LOG_INFO("Connected to socket with ID ", socket.socketIdentifier, " and address ", socket.socket.getIpAddress(),
             ":", socket.socket.getPort());
}

void SocketContainer::handleConnectedSocketEvents(Socket& socket, const uint32_t socketEvents)
{
    if (socketEvents & EPOLLIN)
    {
        LOG_DEBUG("Receiving bytes from socket with ID ", socket.socketIdentifier, " and address ",
                  socket.socket.getIpAddress(), ":", socket.socket.getPort());

        const auto [receivedBytes, closed] = socket.socket.receiveBytes();

        LOG_INFO("Received bytes ", receivedBytes.size(), " from socket with ID ", socket.socketIdentifier,
                 " and address ", socket.socket.getIpAddress(), ":", socket.socket.getPort());

        if (socket.listeningSocketIdentifier != invalidSocketIdentifier)
        {
            auto& listeningSocket = getSocketOrThrow(socket.listeningSocketIdentifier);

            listeningSocket.protocolReader.read(socket.socketIdentifier, receivedBytes);
        }
        else
        {
            socket.protocolReader.read(socket.socketIdentifier, receivedBytes);
        }

        if (closed)
        {
            eraseSocket(socket.socketIdentifier);
        }
    }

    if (socketEvents & EPOLLOUT)
    {
        sendBytes(socket, {});
    }
}

void SocketContainer::handleSocketEventsWork(Socket& socket, const uint32_t socketEvents)
{
    const auto socketIdentifier = socket.socketIdentifier;

    if (socketEvents & EPOLLRDHUP)
    {
        LOG_INFO("Connection was closed for socket with ID ", socketIdentifier, " and address ",
                 socket.socket.getIpAddress(), ':', socket.socket.getPort());

        eraseSocket(socketIdentifier);

        return;
    }

    if (socketEvents & EPOLLERR)
    {
        LOG_WARNING("Connection was aborted for socket with ID ", socketIdentifier, " and address ",
                    socket.socket.getIpAddress(), ':', socket.socket.getPort());

        eraseSocket(socketIdentifier);

        return;
    }

    switch (socket.socket.getSocketType())
    {
    case SocketType::accepted:
    case SocketType::connected:
        handleConnectedSocketEvents(socket, socketEvents);
        break;
    case SocketType::listening:
        handleListeningSocketEvents(socket);
        break;
    case SocketType::connecting:
        handleConnectingSocketEvents(socket);
        break;
    default:
        WTHROW(InternalSocketError, "Invalid socket type");
    }
}

void SocketContainer::handleSocketEvents(const int socketFileDescriptor, const uint32_t socketEvents)
{
    const auto socketPosition = fileDescriptorsToSockets_.find(socketFileDescriptor);

    if (socketPosition == fileDescriptorsToSockets_.end())
    {
        LOG_ERROR("Unsubscribing unused socket");

        eventPoll_.unsubscribe(socketFileDescriptor);

        return;
    }

    const auto socketIdentifier = socketPosition->second->socketIdentifier;

    LOG_DEBUG("Processing events for socket with ID ", socketIdentifier);

    try
    {
        handleSocketEventsWork(*(socketPosition->second), socketEvents);
    }
    catch (const WideException& exception)
    {
        LOG_ERROR("Error processing events for socket with ID ", socketIdentifier, ": ", exception.getMessage());

        eraseSocket(socketIdentifier);
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR("Error processing events for socket with ID ", socketIdentifier, ": ", exception.what());

        eraseSocket(socketIdentifier);
    }
}

}
#endif
