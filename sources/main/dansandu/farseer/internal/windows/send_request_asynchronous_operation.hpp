#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"

#include <any>
#include <vector>

namespace dansandu::farseer::internal::windows::send_request_asynchronous_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation>
createSendRequestAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                       const ProtocolSequenceNumber protocolSequenceNumber,
                                       std::vector<uint8_t>&& bytes,
                                       UniqueFunction<void(std::any&&)>&& expectedResponseConsumer);

}
