#include "dansandu/farseer/common.hpp"
#include "dansandu/journey/exception.hpp"

namespace dansandu::farseer
{

const char* toString(const SocketEvent event)
{
    switch (event)
    {
    case SocketEvent::serverOpen:
        return "serverOpen";
    case SocketEvent::serverClosed:
        return "serverClosed";
    case SocketEvent::serverAborted:
        return "serverAborted";
    case SocketEvent::clientOpen:
        return "clientOpen";
    case SocketEvent::clientBytesReceived:
        return "clientBytesReceived";
    case SocketEvent::clientBytesSent:
        return "clientBytesSent";
    case SocketEvent::clientClosed:
        return "clientClosed";
    case SocketEvent::clientAborted:
        return "clientAborted";
    default:
        THROW(std::logic_error, "Unknown SocketEvent");
    }
}

}
