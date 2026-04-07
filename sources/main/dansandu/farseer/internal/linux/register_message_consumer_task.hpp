#pragma once

#include "dansandu/ballotin/function.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/task.hpp"

#include <any>
#include <memory>
#include <vector>

namespace dansandu::farseer::internal::linux::register_message_consumer_task
{

std::unique_ptr<dansandu::farseer::internal::linux::task::ITask>
createRegisterMessageConsumerTask(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                  UniqueFunction<void(std::any&&)>&& messageConsumer);
}
