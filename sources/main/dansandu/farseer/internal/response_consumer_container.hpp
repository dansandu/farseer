#pragma once

#include "dansandu/farseer/common.hpp"

#include <chrono>
#include <map>
#include <optional>

namespace dansandu::farseer::internal::response_consumer_container
{

class ResponseConsumerContainer
{
public:
    void insert(const ProtocolSequenceNumber sequenceNumber, OneShotResponseConsumer&& responseConsumer);

    void invoke(
        const ProtocolIdentifier responseIdentifier, const ProtocolSequenceNumber sequenceNumber, std::any&& response
    );

    void erase(const ProtocolSequenceNumber sequenceNumber) noexcept;

    // Erases timed out responses and returns the earliest deadline of the remaining responses, if any.
    std::optional<std::chrono::steady_clock::time_point> eraseTimedOutResponses() noexcept;

private:
    void erase(const std::map<ProtocolSequenceNumber, OneShotResponseConsumer>::iterator position) noexcept;

    std::multimap<std::chrono::steady_clock::time_point, ProtocolSequenceNumber> deadlines_;
    std::map<ProtocolSequenceNumber, OneShotResponseConsumer> responseConsumers_;
};

}
