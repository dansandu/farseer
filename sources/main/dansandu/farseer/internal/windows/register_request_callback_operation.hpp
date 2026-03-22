#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/i_operation_scheduler.hpp"

#include <any>

namespace dansandu::farseer::internal::windows::register_request_callback_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::i_operation_scheduler::Operation>
createRegisterRequestCallbackOperation(const SocketIdentifier socketIdentifier,
                                       const ProtocolIdentifier protocolIdentifier,
                                       UniqueFunction<std::any(std::any&&)>&& requestConsumer);

}
