#pragma once

#include <IntSafe.h>

#include <string>

namespace dansandu::farseer::internal::windows::error
{

bool isClientError(const DWORD errorCode) noexcept;

std::string getErrorMessageFromCode(const DWORD errorCode) noexcept;

std::string getLastErrorMessage() noexcept;

std::string getLastWsaErrorMessage() noexcept;

}
