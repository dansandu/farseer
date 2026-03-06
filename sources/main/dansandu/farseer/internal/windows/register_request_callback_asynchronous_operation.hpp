#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"

#include <any>

namespace dansandu::farseer::internal::windows::register_request_callback_asynchronous_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation>
createRegisterRequestCallbackAsynchronousOperation(const SocketServiceId serviceId,
                                                   const ProtocolIdentifier protocolIdentifier,
                                                   UniqueFunction<std::any(std::any&&)>&& requestConsumer);

}
