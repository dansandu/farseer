#include "dansandu/farseer/internal/protocol.hpp"
#include "dansandu/ballotin/exception.hpp"

#include <sstream>

namespace dansandu::farseer::internal::protocol
{

const char* toString(const TypeEnum typeEnum)
{
    switch (typeEnum)
    {
    case TypeEnum::int32:
        return "int32";
    case TypeEnum::int64:
        return "int64";
    case TypeEnum::uint32:
        return "uint32";
    case TypeEnum::uint64:
        return "uint64";
    case TypeEnum::string:
        return "string";
    case TypeEnum::boolean:
        return "boolean";
    case TypeEnum::list:
        return "list";
    case TypeEnum::custom:
        return "custom";
    default:
        THROW(std::logic_error, "unrecognized TypeEnum");
    }
}

Type Type::fromSimple(const TypeEnum typeEnum)
{
    if (typeEnum == TypeEnum::list || typeEnum == TypeEnum::custom)
    {
        THROW(std::logic_error, "this constructor cannot be used for list or custom types");
    }
    auto type = Type{};
    type.typeEnum_ = typeEnum;
    return type;
}

Type Type::fromCustom(const std::string& identifier)
{
    auto type = Type{};
    type.typeEnum_ = TypeEnum::custom;
    type.identifier_ = identifier;
    return type;
}

Type Type::fromList(Type subtype)
{
    auto type = Type{};
    type.typeEnum_ = TypeEnum::list;
    type.subtype_ = std::make_unique<Type>(std::move(subtype));
    return type;
}

Type::Type() : typeEnum_{TypeEnum::int32}
{
}

Type::Type(const Type& other)
    : typeEnum_{other.typeEnum_},
      identifier_{other.identifier_},
      subtype_{other.subtype_ ? std::make_unique<Type>(*other.subtype_) : nullptr}
{
}

Type& Type::operator=(const Type& other)
{
    typeEnum_ = other.typeEnum_;
    identifier_ = other.identifier_;
    subtype_ = other.subtype_ ? std::make_unique<Type>(*other.subtype_) : nullptr;
    return *this;
}

TypeEnum Type::getTypeEnum() const
{
    return typeEnum_;
}

std::string Type::getIdentifier() const
{
    return identifier_;
}

const Type* Type::getSubtype() const
{
    return subtype_.get();
}

std::string Type::getCppType() const
{
    switch (typeEnum_)
    {
    case TypeEnum::int32:
        return "int32_t";
    case TypeEnum::int64:
        return "int64_t";
    case TypeEnum::uint32:
        return "uint32_t";
    case TypeEnum::uint64:
        return "uint64_t";
    case TypeEnum::string:
        return "std::string";
    case TypeEnum::boolean:
        return "bool";
    case TypeEnum::list:
    {
        if (!subtype_)
        {
            THROW(std::logic_error, "subtype cannot be nullptr when type is list");
        }
        return "std::vector<" + subtype_->getCppType() + ">";
    }
    case TypeEnum::custom:
        return identifier_;
    default:
        THROW(std::logic_error, "unrecognized TypeEnum");
    }
}

std::string Type::toString() const
{
    if (typeEnum_ == TypeEnum::custom)
    {
        return identifier_;
    }
    else if (typeEnum_ == TypeEnum::list)
    {
        if (!subtype_)
        {
            THROW(std::logic_error, "subtype cannot be nullptr when type is list");
        }
        return "list<" + subtype_->toString() + ">";
    }
    else
    {
        return dansandu::farseer::internal::protocol::toString(typeEnum_);
    }
}

std::string Protocol::toString() const
{
    auto stream = std::ostringstream{};

    stream << "namespace " << fileNamespace << ";\n\n";

    for (auto messagePosition = messages.cbegin(); messagePosition != messages.cend(); ++messagePosition)
    {
        stream << "message " << messagePosition->identifier << "\n{\n";

        for (const auto& field : messagePosition->fields)
        {
            stream << "    " << field.type.toString() << " " << field.identifier << ";\n";
        }

        stream << "}\n";

        if (messagePosition + 1 != messages.cend())
        {
            stream << std::endl;
        }
    }

    if (!messages.empty() && !requests.empty())
    {
        stream << std::endl;
    }

    for (auto requestPosition = requests.cbegin(); requestPosition != requests.cend(); ++requestPosition)
    {
        stream << "request " << requestPosition->identifier << "\n{\n";

        for (const auto& field : requestPosition->requestFields)
        {
            stream << "    " << field.type.toString() << " " << field.identifier << ";\n";
        }

        stream << "\n    response\n    {\n";

        for (const auto& field : requestPosition->responseFields)
        {
            stream << "        " << field.type.toString() << " " << field.identifier << ";\n";
        }

        stream << "    }\n}\n";

        if (requestPosition + 1 != requests.cend())
        {
            stream << std::endl;
        }
    }

    return stream.str();
}

}
