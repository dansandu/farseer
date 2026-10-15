#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/operation.hpp"

namespace dansandu::farseer::internal::windows::accept_system_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::operation::ISystemOperation> createAcceptSystemOperation(
    const SocketIdentifier listeningSocketIdentifier, const SocketIdentifier pendingAcceptSocketIdentifier
);

}
