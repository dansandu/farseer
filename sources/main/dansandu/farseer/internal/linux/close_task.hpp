#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/task.hpp"

#include <memory>

namespace dansandu::farseer::internal::linux::close_task
{

std::unique_ptr<dansandu::farseer::internal::linux::task::ITask>
createCloseTask(const SocketIdentifier socketIdentifier);

}
