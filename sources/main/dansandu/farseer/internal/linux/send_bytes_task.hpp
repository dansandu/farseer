#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/i_task_scheduler.hpp"

#include <memory>
#include <string>

namespace dansandu::farseer::internal::linux::send_bytes_task
{

std::unique_ptr<dansandu::farseer::internal::linux::i_task_scheduler::ITask>
createSendBytesTask(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes);

}
