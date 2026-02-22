#pragma once

#include "dansandu/farseer/internal/protocol_definition.hpp"

#include <string>

namespace dansandu::farseer::internal::cpp_protocol
{

std::string generateProtocolCppHeader(
    const dansandu::farseer::internal::protocol_definition::ProtocolDefinition& protocolDefinition);

std::string generateProtocolCppSource(
    const dansandu::farseer::internal::protocol_definition::ProtocolDefinition& protocolDefinition);

}
