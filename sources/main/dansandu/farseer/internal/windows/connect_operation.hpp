#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/operation_scheduler.hpp"

namespace dansandu::farseer::internal::windows::connect_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::operation_scheduler::Operation>
createConnectOperation(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                       ConnectionCallback&& connectionCallback);

}
