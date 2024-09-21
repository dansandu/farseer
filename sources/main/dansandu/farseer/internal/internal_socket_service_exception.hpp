#pragma once

#include "dansandu/ballotin/string.hpp"

#include <exception>
#include <string>

namespace dansandu::farseer::internal::internal_socket_service_exception
{

class InternalSocketServiceException : std::exception
{
public:
    explicit InternalSocketServiceException(const std::wstring& message);

    const char* what() const noexcept override;

    const std::wstring& getMessage() const noexcept;

private:
    std::wstring message_;
};

}
