#include "dansandu/farseer/internal/internal_socket_service_exception.hpp"

#include <exception>
#include <string>

namespace dansandu::farseer::internal::internal_socket_service_exception
{

InternalSocketServiceException::InternalSocketServiceException(const std::wstring& message) : message_{message}
{
}

const char* InternalSocketServiceException::what() const noexcept
{
    return "This is a wide character exception, use getMessage() for actual message";
}

const std::wstring& InternalSocketServiceException::getMessage() const noexcept
{
    return message_;
}

}
