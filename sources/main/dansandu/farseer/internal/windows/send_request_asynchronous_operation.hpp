#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"

#include <any>
#include <vector>

namespace dansandu::farseer::internal::windows::send_request_asynchronous_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation>
createSendRequestAsynchronousOperation(const SocketServiceId serviceId, const ProtocolSequenceNumber sequenceNumber,
                                       std::vector<uint8_t>&& bytes,
                                       Function<void(std::any&&)>&& expectedResponseConsumer);

}
