#pragma once

#include "dansandu/farseer/internal/windows/operation.hpp"

namespace dansandu::farseer::internal::windows::listen_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::operation::IOperation>
createListenOperation(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                      ConnectionCallback&& connectionCallback);

}
