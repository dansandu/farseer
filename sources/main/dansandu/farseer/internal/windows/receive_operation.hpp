#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/i_operation_scheduler.hpp"

namespace dansandu::farseer::internal::windows::receive_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::i_operation_scheduler::Operation>
createReceiveOperation(const SocketIdentifier socketIdentifier);

}
