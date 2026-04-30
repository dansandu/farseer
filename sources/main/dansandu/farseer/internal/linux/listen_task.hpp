#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/task.hpp"

#include <memory>
#include <string>

namespace dansandu::farseer::internal::linux::listen_task
{

std::unique_ptr<dansandu::farseer::internal::linux::task::ITask>
createListenTask(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                 UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback);

}
