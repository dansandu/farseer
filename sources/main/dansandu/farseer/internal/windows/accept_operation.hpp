#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/operation.hpp"

namespace dansandu::farseer::internal::windows::accept_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::operation::IOperation>
createAcceptOperation(const SocketIdentifier listeningSocketIdentifier,
                      const SocketIdentifier pendingAcceptSocketIdentifier);

}
