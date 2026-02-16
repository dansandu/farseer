#include "dansandu/farseer/common.hpp"
#include "dansandu/ballotin/binary.hpp"
#include "dansandu/ballotin/exception.hpp"

using dansandu::ballotin::binary::bitsPerByte;

namespace dansandu::farseer
{

ProtocolSize getProtocolSizeFromStdSize(const size_t size)
{
    using UnderlyingType = typename ProtocolSize::UnderlyingType;

    if constexpr (sizeof(size_t) > sizeof(UnderlyingType))
    {
        const auto limit = size_t{1} << (sizeof(UnderlyingType) * bitsPerByte);
        if (size < limit)
        {
            return ProtocolSize{static_cast<UnderlyingType>(size)};
        }
        THROW(std::runtime_error, "The size ", size, " exceeds the protocol size limit of ", limit);
    }
    else if constexpr (sizeof(size_t) == sizeof(UnderlyingType))
    {
        return ProtocolSize{size};
    }
    else
    {
        static_assert(sizeof(size_t) >= sizeof(UnderlyingType), "Protocol size type exceeds standard size type");
    }
}

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
    case SocketServiceEvent::clientBytesReceived:
        return "clientBytesReceived";
    case SocketServiceEvent::clientBytesSent:
        return "clientBytesSent";
    case SocketServiceEvent::clientClosed:
        return "clientClosed";
    case SocketServiceEvent::clientAborted:
        return "clientAborted";
    default:
        THROW(std::logic_error, "Unknown SocketServiceEvent");
    }
}

}
