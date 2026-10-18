#pragma once

#include <cstddef>

namespace dansandu::farseer::internal::receive_buffer
{

// Number of bytes read from a socket in a single receive call. Larger messages are reassembled from multiple reads, so
// this doesn't limit the size of a message.
constexpr auto maximumReceiveBufferSize = 16384uz;

}
