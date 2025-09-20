#pragma once

#include <stdexcept>

namespace dansandu::farseer::exception
{

class ProtocolValidationError : public std::runtime_error
{
public:
    using runtime_error::runtime_error;
};

class ReservedIdentifierNameError : public std::runtime_error
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

class DuplicateProtocolIdentifierError : public ProtocolValidationError
{
public:
    using ProtocolValidationError::ProtocolValidationError;
};

class MessageIdentifierNotDefinedError : public ProtocolValidationError
{
public:
    using ProtocolValidationError::ProtocolValidationError;
};

class ProtocolFieldSelfReferenceError : public ProtocolValidationError
{
public:
    using ProtocolValidationError::ProtocolValidationError;
};

class DuplicateFieldIdentifierError : public ProtocolValidationError
{
public:
    using ProtocolValidationError::ProtocolValidationError;
};

}
