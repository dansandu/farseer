#pragma once

#include "dansandu/farseer/internal/protocol_definition.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dansandu::farseer::internal::protocol_parsing
{

dansandu::farseer::internal::protocol_definition::ProtocolFile parseProtocolFile(const std::string_view text);

}
