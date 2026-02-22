#pragma once

#include "dansandu/ballotin/string.hpp"
#include "dansandu/journey/exception.hpp"

#include <stdexcept>

namespace dansandu::farseer::exception
{

class RequestProtocolError : public std::exception
{
public:
    RequestProtocolError(const uint32_t errorCode, const std::string& errorMessage)
        : errorCode_{errorCode},
          errorMessage_{errorMessage},
          message_{dansandu::ballotin::string::format("Error code: ", errorCode, ", error message: ", errorMessage)}
    {
    }

    uint32_t getErrorCode() const noexcept
    {
        return errorCode_;
    }

    const std::string& getErrorMessage() const noexcept
    {
        return errorMessage_;
    }

    const char* what() const noexcept override
    {
        return message_.c_str();
    }

private:
    uint32_t errorCode_;
    std::string errorMessage_;
    std::string message_;
};

class ProtocolValidationError : public std::runtime_error
{
public:
    using runtime_error::runtime_error;
};

class ReservedNameError : public std::runtime_error
{
public:
    using runtime_error::runtime_error;
};

class ProtocolIdentifierAlreadyRegisteredError : public std::runtime_error
{
public:
    using runtime_error::runtime_error;
};

class ProtocolNotRegisteredError : public std::runtime_error
{
public:
    using runtime_error::runtime_error;
};

class ProtocolConsumerAlreadyRegisteredError : public std::runtime_error
{
public:
    using runtime_error::runtime_error;
};

class DuplicateProtocolNameError : public ProtocolValidationError
{
public:
    using ProtocolValidationError::ProtocolValidationError;
};

class MessageNameNotDefinedError : public ProtocolValidationError
{
public:
    using ProtocolValidationError::ProtocolValidationError;
};

class ProtocolFieldSelfReferenceError : public ProtocolValidationError
{
public:
    using ProtocolValidationError::ProtocolValidationError;
};

class DuplicateFieldNameError : public ProtocolValidationError
{
public:
    using ProtocolValidationError::ProtocolValidationError;
};

class InternalSocketServiceException : public dansandu::journey::exception::WideException
{
public:
    using WideException::WideException;
};

}
