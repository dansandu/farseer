#pragma once

#include <functional>
#include <vector>

namespace dansandu::farseer
{

class SocketServiceId
{
public:
    friend constexpr auto operator<=>(const SocketServiceId& left, const SocketServiceId& right) = default;

    using IntegerType = unsigned long long;

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
    clientMessageReceived,
    clientMessageSent,
    clientClosed,
    clientAborted,
};

PRALINE_EXPORT const char* toString(const SocketServiceEvent event);

using ByteType = char;

using BytesType = std::vector<ByteType>;

using CallbackType = std::function<void(const SocketServiceEvent event, const SocketServiceId serverId,
                                        const SocketServiceId clientId, BytesType message)>;

}
