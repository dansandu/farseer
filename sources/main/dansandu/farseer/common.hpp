#pragma once

#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

namespace dansandu::farseer
{

class ProtocolIdentifier
{
public:
    friend constexpr auto operator<=>(const ProtocolIdentifier& left, const ProtocolIdentifier& right) = default;

    friend std::ostream& operator<<(std::ostream& stream, const ProtocolIdentifier protocolIdentifier)
    {
        return stream << protocolIdentifier.integer_;
    }

    using IntegerType = uint32_t;

    constexpr ProtocolIdentifier() : integer_{0}
    {
    }

    constexpr explicit ProtocolIdentifier(const IntegerType integer) : integer_{integer}
    {
    }

    constexpr IntegerType getInteger() const
    {
        return integer_;
    }

    std::string toString() const
    {
        return std::to_string(integer_);
    }

private:
    IntegerType integer_;
};

class SocketServiceId
{
public:
    friend constexpr auto operator<=>(const SocketServiceId& left, const SocketServiceId& right) = default;

    friend std::ostream& operator<<(std::ostream& stream, const SocketServiceId serviceId)
    {
        return stream << serviceId.integer_;
    }

    using IntegerType = unsigned long;

    constexpr SocketServiceId() : integer_{0}
    {
    }

    constexpr explicit SocketServiceId(IntegerType integer) : integer_{integer}
    {
    }

    constexpr IntegerType getInteger() const
    {
        return integer_;
    }

private:
    IntegerType integer_;
};

static constexpr SocketServiceId InvalidServiceId = SocketServiceId{};

enum class SocketServiceEvent
{
    serverOpen,
    serverClosed,
    serverAborted,
    clientOpen,
    clientBytesReceived,
    clientBytesSent,
    clientClosed,
    clientAborted,
};

PRALINE_EXPORT const char* toString(const SocketServiceEvent event);

using BytesType = std::vector<uint8_t>;

using ConnectionCallbackType =
    std::function<void(const SocketServiceEvent event, const SocketServiceId serverId, const SocketServiceId clientId)>;

}
