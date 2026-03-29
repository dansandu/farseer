#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/i_task_scheduler.hpp"

#include <memory>
#include <string>

namespace dansandu::farseer::internal::linux::listen_task
{

std::unique_ptr<dansandu::farseer::internal::linux::i_task_scheduler::ITask>
createListenTask(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                 ConnectionCallback&& connectionCallback);

}
