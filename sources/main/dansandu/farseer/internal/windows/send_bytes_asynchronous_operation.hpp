#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"

namespace dansandu::farseer::internal::windows::send_bytes_asynchronous_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation>
createSendBytesAsynchronousOperation(const SocketServiceId serviceId, std::vector<uint8_t> bytes);

}
