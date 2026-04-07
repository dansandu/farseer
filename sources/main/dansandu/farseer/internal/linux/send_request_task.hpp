#pragma once

#include "dansandu/ballotin/function.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/task.hpp"

#include <any>
#include <memory>
#include <vector>

namespace dansandu::farseer::internal::linux::send_request_task
{

std::unique_ptr<dansandu::farseer::internal::linux::task::ITask>
createSendRequestTask(const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber protocolSequenceNumber,
                      std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer);

}
