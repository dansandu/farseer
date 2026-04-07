#pragma once

#include "dansandu/ballotin/function.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/task.hpp"

#include <any>
#include <memory>
#include <vector>

namespace dansandu::farseer::internal::linux::register_request_callback_task
{

std::unique_ptr<dansandu::farseer::internal::linux::task::ITask>
createRegisterRequestCallbackTask(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                  UniqueFunction<std::any(std::any&&)>&& requestConsumer);
}
