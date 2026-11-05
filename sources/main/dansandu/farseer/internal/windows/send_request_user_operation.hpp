#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/operation.hpp"

#include <any>
#include <chrono>
#include <optional>
#include <vector>

namespace dansandu::farseer::internal::windows::send_request_user_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::operation::IUserOperation> createSendRequestUserOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier responseIdentifier,
    const ProtocolSequenceNumber sequenceNumber, const std::optional<std::chrono::steady_clock::time_point> deadline,
    std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer
);

}
