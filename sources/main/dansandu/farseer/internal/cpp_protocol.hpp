#pragma once

#include "dansandu/farseer/internal/protocol_definition.hpp"

#include <string>

namespace dansandu::farseer::internal::cpp_protocol
{

std::string generateCppProtocol(const dansandu::farseer::internal::protocol_definition::ProtocolFile& protocolFile);

}
