#pragma once

#include "dansandu/farseer/internal/protocol.hpp"

#include <string_view>

namespace dansandu::farseer::internal::protocol_parsing
{

dansandu::farseer::internal::protocol::Protocol parseProtocol(const std::string_view text);

}
