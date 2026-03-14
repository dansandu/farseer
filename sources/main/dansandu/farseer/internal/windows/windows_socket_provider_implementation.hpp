#pragma once

#include "dansandu/farseer/internal/socket_provider_implementation.hpp"

#include <memory>

namespace dansandu::farseer::internal::windows::windows_socket_provider_implementation
{

std::shared_ptr<dansandu::farseer::internal::socket_provider_implementation::ISocketProviderImplementation>
createWindowsSocketProviderImplementation(const bool initializeWsa);

}
