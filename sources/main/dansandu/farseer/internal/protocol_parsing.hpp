#pragma once

#include "dansandu/farseer/internal/protocol_definition.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dansandu::farseer::internal::protocol_parsing
{

class ProtocolValidationError : public std::exception
{
public:
    explicit ProtocolValidationError(const std::string& message) : message_{message}
    {
    }

    const char* what() const noexcept override
    {
        return message_.c_str();
    }

private:
    std::string message_;
};

dansandu::farseer::internal::protocol_definition::ProtocolFile parseProtocolFile(const std::string_view text);

}
