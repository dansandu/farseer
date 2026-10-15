#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/operation.hpp"

namespace dansandu::farseer::internal::windows::connect_user_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::operation::IUserOperation> createConnectUserOperation(
    const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
);

}
