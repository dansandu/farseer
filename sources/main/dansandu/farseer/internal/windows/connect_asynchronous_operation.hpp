#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"

namespace dansandu::farseer::internal::windows::connect_asynchronous_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation>
createConnectAsynchronousOperation(const SocketIdentifier socketIdentifier, const std::wstring& ipAddress,
                                   const int port, ConnectionCallback&& connectionCallback);

}
