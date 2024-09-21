#pragma once

#include <IntSafe.h>

#include <string>

namespace dansandu::farseer::internal::error
{

std::string getErrorMessageFromCode(DWORD errorCode);

std::string getLastErrorMessage();

std::string getLastWsaErrorMessage();

}
