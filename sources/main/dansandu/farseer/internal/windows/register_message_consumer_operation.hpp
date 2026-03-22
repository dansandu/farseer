#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/i_operation_scheduler.hpp"

#include <any>

namespace dansandu::farseer::internal::windows::register_message_consumer_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::i_operation_scheduler::Operation>
createRegisterMessageConsumerOperation(const SocketIdentifier socketIdentifier,
                                       const ProtocolIdentifier protocolIdentifier,
                                       UniqueFunction<void(std::any&&)>&& messageConsumer);

}
