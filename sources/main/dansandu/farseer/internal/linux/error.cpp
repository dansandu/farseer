#if defined(__linux__)
#include "dansandu/farseer/internal/linux/error.hpp"
#include "dansandu/ballotin/string.hpp"

#include <errno.h>
#include <string.h>
#include <string>

using dansandu::ballotin::string::format;

namespace dansandu::farseer::internal::linux::error
{

std::string getLastErrorMessage()
{
    const auto lastErrorCode = errno;
    errno = 0;
    const auto errorMessage = ::strerror(lastErrorCode);
    if (errno == 0)
    {
        const auto message = format(lastErrorCode, " ", errorMessage);
        return message;
    }
    return "Failed to generate error message from error code";
}

}
#endif
