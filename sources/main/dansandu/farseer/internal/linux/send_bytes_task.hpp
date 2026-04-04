#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/task.hpp"

#include <memory>
#include <vector>

namespace dansandu::farseer::internal::linux::send_bytes_task
{

std::unique_ptr<dansandu::farseer::internal::linux::task::ITask>
createSendBytesTask(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes);

}
