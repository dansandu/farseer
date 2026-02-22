#pragma once

#include "dansandu/farseer/internal/protocol_definition.hpp"

#include <string_view>

namespace dansandu::farseer::internal::protocol_definition_parsing
{

dansandu::farseer::internal::protocol_definition::ProtocolDefinition
parseProtocolDefinition(const std::string_view text);

}
