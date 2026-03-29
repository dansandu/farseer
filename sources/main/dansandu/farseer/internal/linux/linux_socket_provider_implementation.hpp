#pragma once

#include "dansandu/farseer/internal/socket_provider_implementation.hpp"

#include <memory>

namespace dansandu::farseer::internal::linux::linux_socket_provider_implementation
{

std::shared_ptr<dansandu::farseer::internal::socket_provider_implementation::ISocketProviderImplementation>
createLinuxSocketProviderImplementation();

}
