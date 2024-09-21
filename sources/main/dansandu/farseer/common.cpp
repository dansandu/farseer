#include "dansandu/farseer/common.hpp"
#include "dansandu/ballotin/exception.hpp"

namespace dansandu::farseer
{

const char* toString(const SocketServiceEvent event)
{
    switch (event)
    {
    case SocketServiceEvent::serverOpen:
        return "serverOpen";
    case SocketServiceEvent::serverClosed:
        return "serverClosed";
    case SocketServiceEvent::serverAborted:
        return "serverAborted";
    case SocketServiceEvent::clientOpen:
        return "clientOpen";
    case SocketServiceEvent::clientMessageReceived:
        return "clientMessageReceived";
    case SocketServiceEvent::clientMessageSent:
        return "clientMessageSent";
    case SocketServiceEvent::clientClosed:
        return "clientClosed";
    case SocketServiceEvent::clientAborted:
        return "clientAborted";
    default:
        THROW(std::logic_error, "Unknown SocketServiceEvent");
    }
}

}
