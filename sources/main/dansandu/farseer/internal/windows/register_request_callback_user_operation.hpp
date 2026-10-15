#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/operation.hpp"

#include <any>

namespace dansandu::farseer::internal::windows::register_request_callback_user_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::operation::IUserOperation>
createRegisterRequestCallbackUserOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<std::any(std::any&&)>&& requestConsumer
);

}
