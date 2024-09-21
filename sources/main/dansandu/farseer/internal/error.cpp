#include "dansandu/farseer/internal/error.hpp"
#include "dansandu/ballotin/logging.hpp"
#include "dansandu/ballotin/string.hpp"

#include <strsafe.h>
#include <windows.h>

using dansandu::ballotin::logging::LogError;
using dansandu::ballotin::logging::LogWarn;
using dansandu::ballotin::string::format;
using dansandu::ballotin::string::trim;

namespace dansandu::farseer::internal::error
{

std::string getErrorMessageFromCode(DWORD errorCode)
{
    const auto flags =
        DWORD{FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS};
    const auto source = LPCVOID{nullptr};
    const auto languageId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    const auto size = 0;

    char* messageBuffer = nullptr;

    const auto formatResult =
        ::FormatMessageA(flags, source, errorCode, languageId, (LPSTR)&messageBuffer, size, nullptr);
    if (formatResult == 0)
    {
        LogError("FormatMessageA failed with error code ", ::GetLastError());
        return {};
    }

    const auto message = trim(format(errorCode, ' ', messageBuffer));

    ::LocalFree(messageBuffer);

    return message;
}

std::string getLastErrorMessage()
{
    const auto errorCode = ::GetLastError();
    if (errorCode == ERROR_SUCCESS)
    {
        LogWarn("getLastErrorMessage was called but there are no errors");
        return {};
    }

    return getErrorMessageFromCode(errorCode);
}

std::string getLastWsaErrorMessage()
{
    const auto errorCode = ::WSAGetLastError();
    if (errorCode == ERROR_SUCCESS)
    {
        LogWarn("getLastWsaErrorMessage was called but there are no errors");
        return {};
    }

    return getErrorMessageFromCode(errorCode);
}

}
