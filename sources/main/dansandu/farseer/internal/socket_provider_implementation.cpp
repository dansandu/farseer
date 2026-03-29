#include "dansandu/farseer/internal/socket_provider_implementation.hpp"

#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/windows_socket_provider_implementation.hpp"
#elif defined(__linux__)
#include "dansandu/farseer/internal/linux/linux_socket_provider_implementation.hpp"
#else
#error "Unknown platform"
#endif

namespace dansandu::farseer::internal::socket_provider_implementation
{

std::shared_ptr<ISocketProviderImplementation> createSocketProviderImplementation(const bool initializeWsa)
{
#if defined(_WIN32)
    return dansandu::farseer::internal::windows::windows_socket_provider_implementation::
        createWindowsSocketProviderImplementation(initializeWsa);
#elif defined(__linux__)
    static_cast<void>(initializeWsa);
    return dansandu::farseer::internal::linux::linux_socket_provider_implementation::
        createLinuxSocketProviderImplementation();
#else
#error "Unknown platform"
#endif
}

}
