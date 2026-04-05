#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/event_poll.hpp"
#include "dansandu/farseer/internal/linux/linux_socket.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace dansandu::farseer::internal::linux::socket_container
{

struct Socket
{
    SocketIdentifier socketIdentifier;
    SocketIdentifier listeningSocketIdentifier;
    dansandu::farseer::internal::linux::linux_socket::LinuxSocket socket;
    dansandu::farseer::internal::protocol_reader::ProtocolReader protocolReader;
    ConnectionCallback connectionCallback;
};

class SocketContainer
{
public:
    SocketContainer(const SocketContainer&) = delete;
    SocketContainer(SocketContainer&& other) noexcept = delete;
    SocketContainer& operator=(const SocketContainer&) = delete;
    SocketContainer& operator=(SocketContainer&& other) noexcept = delete;

    explicit SocketContainer(
        dansandu::farseer::internal::linux::event_poll::EventPoll& eventPoll,
        dansandu::farseer::internal::sequencer::Sequencer<SocketIdentifier>& socketIdentifierSequencer);

    ~SocketContainer() noexcept;

    void listen(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                ConnectionCallback&& connectionCallback);

    void connect(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                 ConnectionCallback&& connectionCallback);

    void sendBytes(const SocketIdentifier socketIdentifier, const std::span<const uint8_t> bytes);

    void eraseSocket(const SocketIdentifier socketIdentifier);

    void handleSocketEvents(const int socketFileDescriptor, const uint32_t socketEvents);

    size_t getNumberOfSockets() const
    {
        return sockets_.size();
    }

private:
    Socket& insertSocket(const uint32_t events, Socket&& socket);

    Socket& getSocketOrThrow(const SocketIdentifier socketIdentifier);

    void handleListeningSocketEvents(Socket& socket);

    void handleConnectingSocketEvents(Socket& socket);

    void handleConnectedSocketEvents(Socket& socket, const uint32_t socketEvents);

    void handleSocketEventsWork(Socket& socket, const uint32_t socketEvents);

    dansandu::farseer::internal::linux::event_poll::EventPoll& eventPoll_;
    dansandu::farseer::internal::sequencer::Sequencer<SocketIdentifier>& socketIdentifierSequencer_;
    std::map<SocketIdentifier, Socket> sockets_;
    std::map<int, Socket*> fileDescriptorsToSockets_;
};

}
