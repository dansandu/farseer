#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/operation_scheduler.hpp"

#include <vector>

namespace dansandu::farseer::internal::windows::send_bytes_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::operation_scheduler::Operation>
createSendBytesOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes);

}
