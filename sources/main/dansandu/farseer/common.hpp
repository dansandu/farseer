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
        return stream << protocolIdentifier.identifier_;
    }

    using ValueType = uint32_t;

    constexpr ProtocolIdentifier() : identifier_{0}
    {
    }

    constexpr explicit ProtocolIdentifier(const ValueType identifier) : identifier_{identifier}
    {
    }

    constexpr ValueType getValue() const
    {
        return identifier_;
    }

    std::string toString() const
    {
        return std::to_string(identifier_);
    }

private:
    ValueType identifier_;
};

class SocketServiceId
{
public:
    friend constexpr auto operator<=>(const SocketServiceId& left, const SocketServiceId& right) = default;

    using IntegerType = unsigned long;

    constexpr SocketServiceId() : integer_{0}
    {
    }

    constexpr explicit SocketServiceId(IntegerType integer) : integer_{integer}
    {
    }

    constexpr IntegerType integer() const
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

using CallbackType = std::function<void(const SocketServiceEvent event, const SocketServiceId serverId,
                                        const SocketServiceId clientId, BytesType bytes)>;

}
