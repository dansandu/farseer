#include "dansandu/farseer/internal/response_consumer_container.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/journey/logging.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <optional>
#include <stdexcept>

using dansandu::farseer::exception::ProtocolConsumerAlreadyRegisteredError;

namespace dansandu::farseer::internal::response_consumer_container
{

void ResponseConsumerContainer::insert(
    const ProtocolSequenceNumber sequenceNumber, OneShotResponseConsumer&& responseConsumer
)
{
    const auto [position, inserted] = responseConsumers_.try_emplace(sequenceNumber, std::move(responseConsumer));

    if (!inserted)
    {
        THROW(
            ProtocolConsumerAlreadyRegisteredError,
            "Another response consumer is already registered with sequence number ", sequenceNumber
        );
    }

    const auto deadline = position->second.getDeadline();

    if (deadline.has_value())
    {
        deadlines_.emplace(deadline.value(), sequenceNumber);
    }
}

void ResponseConsumerContainer::invoke(
    const ProtocolIdentifier responseIdentifier, const ProtocolSequenceNumber sequenceNumber, std::any&& response
)
{
    const auto position = responseConsumers_.find(sequenceNumber);

    if (position != responseConsumers_.cend())
    {
        SCOPE_EXIT([&]() { erase(position); });

        if (!position->second.invoke(std::move(response)))
        {
            THROW(
                std::logic_error, "The response consumer with identifier ", responseIdentifier, " and sequence number ",
                sequenceNumber, " was already invoked"
            );
        }
    }
    else
    {
        LOG_WARNING(
            "The response protocol with identifier ", responseIdentifier, " and sequence number ", sequenceNumber,
            " will be skipped because no consumer was found possibly due to a timeout"
        );
    }
}

void ResponseConsumerContainer::erase(
    const std::map<ProtocolSequenceNumber, OneShotResponseConsumer>::iterator position
) noexcept
{
    const auto deadline = position->second.getDeadline();

    if (deadline.has_value())
    {
        const auto [first, last] = deadlines_.equal_range(deadline.value());

        const auto deadlinePosition =
            std::find_if(first, last, [position](const auto& entry) { return entry.second == position->first; });

        if (deadlinePosition != last)
        {
            deadlines_.erase(deadlinePosition);
        }
    }

    responseConsumers_.erase(position);
}

void ResponseConsumerContainer::erase(const ProtocolSequenceNumber sequenceNumber) noexcept
{
    const auto position = responseConsumers_.find(sequenceNumber);

    if (position != responseConsumers_.cend())
    {
        erase(position);
    }
}

std::optional<std::chrono::steady_clock::time_point> ResponseConsumerContainer::eraseTimedOutResponses() noexcept
{
    const auto now = std::chrono::steady_clock::now();

    const auto firstPendingPosition = deadlines_.upper_bound(now);

    for (auto iterator = deadlines_.begin(); iterator != firstPendingPosition; ++iterator)
    {
        responseConsumers_.erase(iterator->second);
    }

    deadlines_.erase(deadlines_.begin(), firstPendingPosition);

    if (deadlines_.empty())
    {
        return std::nullopt;
    }

    return deadlines_.begin()->first;
}

}
