#if defined(__linux__)
#include "dansandu/farseer/internal/linux/error.hpp"
#include "dansandu/ballotin/string.hpp"

#include <errno.h>
#include <string.h>
#include <string>

using dansandu::ballotin::string::format;

namespace dansandu::farseer::internal::linux::error
{

std::string getErrorMessage(const int errorCode)
{
    const auto errorName = ::strerrorname_np(errorCode);

    if (!errorName)
    {
        return "Failed to generate error name from error code";
    }

    errno = 0;

    const auto errorDescription = ::strerror(errorCode);

    if (errno)
    {
        return "Failed to generate error message from error code";
    }

    const auto message = format(errorName, "(", errorCode, ") ", errorDescription);

    return message;
}

std::string getLastErrorMessage()
{
    return getErrorMessage(errno);
}

}
#endif
