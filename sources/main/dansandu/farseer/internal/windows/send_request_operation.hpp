#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/operation.hpp"

#include <any>
#include <vector>

namespace dansandu::farseer::internal::windows::send_request_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::operation::IOperation>
createSendRequestOperation(const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber protocolSequenceNumber,
                           std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& expectedResponseConsumer);

}
