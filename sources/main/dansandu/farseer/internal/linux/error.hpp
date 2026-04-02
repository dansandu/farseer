#pragma once

#include <string>

namespace dansandu::farseer::internal::linux::error
{

std::string getErrorMessage(const int errorCode);

std::string getLastErrorMessage();

}
