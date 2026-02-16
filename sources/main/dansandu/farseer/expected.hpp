#pragma once

#include <string>
#include <variant>

namespace dansandu::farseer::expected
{

constexpr uint32_t internalServerErrorCode = 0;

template<typename T>
class Expected
{
    struct Error
    {
        uint32_t errorCode;
        std::string errorMessage;
    };

public:
    template<typename U>
    static Expected fromSuccess(U&& value)
    {
        return Expected{std::in_place_type<T>, std::forward<U>(value)};
    }

    static Expected fromFailure(const uint32_t errorCode, const std::string& errorMessage)
    {
        return Expected{std::in_place_type<Error>, Error{errorCode, errorMessage}};
    }

    static Expected fromInternalServerError()
    {
        return fromFailure(internalServerErrorCode, "Internal server error");
    }

    static Expected fromInternalServerError(const std::string& errorMessage)
    {
        return fromFailure(internalServerErrorCode, "Internal server error: " + errorMessage);
    }

    bool success() const
    {
        return std::holds_alternative<T>(value_);
    }

    bool failure() const
    {
        return std::holds_alternative<Error>(value_);
    }

    const T& getValue() const
    {
        return std::get<T>(value_);
    }

    uint32_t getErrorCode() const
    {
        return std::get<Error>(value_).errorCode;
    }

    const std::string& getErrorMessage() const
    {
        return std::get<Error>(value_).errorMessage;
    }

private:
    template<typename U, typename V>
    Expected(std::in_place_type_t<U> tag, V&& value) : value_{tag, std::forward<V>(value)}
    {
    }

    std::variant<T, Error> value_;
};

}
