#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/task.hpp"

#include <memory>
#include <string>

namespace dansandu::farseer::internal::linux::connect_task
{

std::unique_ptr<dansandu::farseer::internal::linux::task::ITask>
createConnectTask(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                  ConnectionCallback&& connectionCallback);

}
