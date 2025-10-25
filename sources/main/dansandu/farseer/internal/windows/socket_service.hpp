#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

#include <map>

namespace dansandu::farseer::internal::windows::socket_service
{

struct SocketService
{
    dansandu::farseer::internal::windows::windows_socket::WindowsSocket socket;
    dansandu::farseer::internal::protocol_reader::ProtocolReader protocolReader;
    SocketServiceId listeningServiceId;
    ConnectionCallbackType connectionCallback;
};

using SocketServiceContainer = std::map<SocketServiceId, SocketService>;
using SocketServiceContainerIterator = typename SocketServiceContainer::iterator;

SocketServiceContainerIterator getServiceOrThrow(SocketServiceContainer& services, const SocketServiceId serviceId);

void closeSocketService(SocketServiceContainer& services, const SocketServiceId serviceId);

}
