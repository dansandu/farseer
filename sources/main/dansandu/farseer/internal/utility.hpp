#pragma once

#include "dansandu/farseer/common.hpp"

#include <chrono>
#include <map>
#include <optional>

namespace dansandu::farseer::internal::utility
{

template<typename SocketImplementation>
std::optional<std::chrono::steady_clock::time_point>
getEarliestResponseDeadline(const std::map<SocketIdentifier, SocketImplementation>& sockets) noexcept
{
    auto earliestDeadline = std::optional<std::chrono::steady_clock::time_point>{};

    for (const auto& entry : sockets)
    {
        const auto socketEarliestDeadline = entry.second.protocolReader.getEarliestResponseDeadline();

        if (socketEarliestDeadline.has_value() &&
            (!earliestDeadline.has_value() || earliestDeadline.value() > socketEarliestDeadline.value()))
        {
            earliestDeadline = socketEarliestDeadline;
        }
    }

    return earliestDeadline;
}

}
