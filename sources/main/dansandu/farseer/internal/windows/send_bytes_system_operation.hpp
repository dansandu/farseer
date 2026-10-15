#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/operation.hpp"

#include <vector>

namespace dansandu::farseer::internal::windows::send_bytes_system_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::operation::ISystemOperation>
createSendBytesSystemOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes);

}
