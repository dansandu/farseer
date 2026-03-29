#include "dansandu/farseer/common.hpp"
#include "dansandu/ballotin/binary.hpp"
#include "dansandu/ballotin/exception.hpp"

#include <type_traits>

using dansandu::ballotin::binary::bitsPerByte;

namespace dansandu::farseer
{

ProtocolSize getProtocolSizeFromStdSize(const size_t size)
{
    using UnderlyingType = typename ProtocolSize::UnderlyingType;

    static_assert(std::is_unsigned_v<UnderlyingType>);

    if constexpr (sizeof(size_t) > sizeof(UnderlyingType))
    {
        const auto limit = 1uz << (sizeof(UnderlyingType) * bitsPerByte);
        if (size < limit)
        {
            return ProtocolSize{static_cast<UnderlyingType>(size)};
        }
        THROW(std::runtime_error, "The size ", size, " exceeds the protocol size limit of ", limit);
    }
    else if constexpr (std::is_same_v<size_t, UnderlyingType>)
    {
        return ProtocolSize{static_cast<UnderlyingType>(size)};
    }
    else
    {
        static_assert(sizeof(size_t) >= sizeof(UnderlyingType), "Protocol size type exceeds standard size type");
    }
}

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
