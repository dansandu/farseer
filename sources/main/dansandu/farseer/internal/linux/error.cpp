#if defined(__linux__)
#include "dansandu/farseer/internal/linux/error.hpp"

#include <errno.h>
#include <string.h>
#include <string>

namespace dansandu::farseer::internal::linux::error
{

std::string getLastErrorMessage()
{
    const auto lastErrorCode = errno;
    errno = 0;
    const auto errorMessage = ::strerror(lastErrorCode);
    if (errno == 0)
    {
        return errorMessage;
    }
    return "Failed to generate error message from error code";
}

}
#endif
