#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"

#include <any>
#include <functional>

namespace dansandu::farseer::internal::windows::register_message_consumer_asynchronous_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation>
createRegisterMessageConsumerAsynchronousOperation(const SocketServiceId serviceId,
                                                   const ProtocolIdentifier protocolIdentifier,
                                                   std::function<void(std::any)> messageConsumer);

}
