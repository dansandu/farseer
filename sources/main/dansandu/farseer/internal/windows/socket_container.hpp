#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

#include <map>
#include <vector>

namespace dansandu::farseer::internal::windows::socket_container
{

struct Socket
{
    dansandu::farseer::internal::windows::windows_socket::WindowsSocket socket;
    dansandu::farseer::internal::protocol_reader::ProtocolReader protocolReader;
    SocketIdentifier listeningSocketIdentifier;
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)> connectionCallback;
    std::vector<uint8_t> inboundBytes;
    std::vector<uint8_t> outboundBytes;
    // Sockets waiting for AcceptEx to complete were never reported as open, so they are not reported as closed either.
    bool pendingAccept;
};

class SocketContainer
{
public:
    Socket& insertSocket(const SocketIdentifier socketIdentifier, Socket&& socket);

    Socket& getSocketOrThrow(const SocketIdentifier socketIdentifier);

    void eraseSocket(const SocketIdentifier socketIdentifier);

    void eraseAllSockets();

private:
    std::map<SocketIdentifier, Socket> sockets_;
};

}
