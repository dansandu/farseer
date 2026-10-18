#pragma once

#include <IntSafe.h>

#include <string>

namespace dansandu::farseer::internal::windows::error
{

DWORD getLastErrorCode() noexcept;

std::string getErrorMessageFromCode(DWORD errorCode) noexcept;

std::string getLastErrorMessage() noexcept;

std::string getLastWsaErrorMessage() noexcept;

}
