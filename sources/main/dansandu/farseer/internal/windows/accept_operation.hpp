#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/i_operation_scheduler.hpp"

namespace dansandu::farseer::internal::windows::accept_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::i_operation_scheduler::Operation>
createAcceptOperation(const SocketIdentifier pendingAcceptSocketIdentifier,
                      const SocketIdentifier listeningSocketIdentifier);

}
