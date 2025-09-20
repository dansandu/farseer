#pragma once

#include "dansandu/farseer/internal/protocol.hpp"

#include <string>

namespace dansandu::farseer::internal::cpp_protocol
{

std::string generateProtocolCppHeader(const dansandu::farseer::internal::protocol::Protocol& protocol);

std::string generateProtocolCppSource(const dansandu::farseer::internal::protocol::Protocol& protocol);

}
