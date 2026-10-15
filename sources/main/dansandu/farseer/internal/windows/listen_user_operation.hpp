#pragma once

#include "dansandu/farseer/internal/windows/operation.hpp"

namespace dansandu::farseer::internal::windows::listen_user_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::operation::IUserOperation> createListenUserOperation(
    const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
);

}
