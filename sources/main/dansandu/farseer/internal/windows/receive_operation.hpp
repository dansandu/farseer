#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/operation.hpp"

namespace dansandu::farseer::internal::windows::receive_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::operation::IOperation>
createReceiveOperation(const SocketIdentifier socketIdentifier);

}
