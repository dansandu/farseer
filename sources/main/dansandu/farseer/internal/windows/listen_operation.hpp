#pragma once

#include "dansandu/farseer/internal/windows/i_operation_scheduler.hpp"

namespace dansandu::farseer::internal::windows::listen_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::i_operation_scheduler::Operation>
createListenOperation(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                      ConnectionCallback&& connectionCallback);

}
