#include "dansandu/farseer/internal/protocol.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/ballotin/hashing.hpp"

#include <sstream>

using dansandu::ballotin::hashing::hashCombine;

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
        return "bool";
    case TypeEnum::list:
        return "list";
    case TypeEnum::message:
        return "message";
    default:
        THROW(std::logic_error, "unrecognized TypeEnum");
    }
}

uint64_t getNumberOfBits(const TypeEnum typeEnum)
{
    switch (typeEnum)
    {
    case TypeEnum::int32:
        return 32;
    case TypeEnum::int64:
        return 64;
    case TypeEnum::uint32:
        return 32;
    case TypeEnum::uint64:
        return 64;
    case TypeEnum::string:
        THROW(std::logic_error, "string is not a static type");
    case TypeEnum::boolean:
        return 1;
    case TypeEnum::list:
        THROW(std::logic_error, "list is not a static type");
    case TypeEnum::message:
        THROW(std::logic_error, "message is not a static type");
    default:
        THROW(std::logic_error, "unrecognized TypeEnum");
    }
}

Type Type::fromSimple(const TypeEnum typeEnum)
{
    if (typeEnum == TypeEnum::list || typeEnum == TypeEnum::message)
    {
        THROW(std::logic_error, "this constructor cannot be used for list or message types");
    }

    auto type = Type{};
    type.typeEnum_ = typeEnum;

    if (typeEnum == TypeEnum::string)
    {
        type.hasStaticSize_ = false;
        type.numberOfBits_ = 0;
    }
    else
    {
        type.hasStaticSize_ = true;
        type.numberOfBits_ = dansandu::farseer::internal::protocol::getNumberOfBits(typeEnum);
    }

    return type;
}

Type Type::fromMessage(const std::string& identifier, bool hasStaticSize, uint64_t numberOfBits)
{
    auto type = Type{};
    type.typeEnum_ = TypeEnum::message;
    type.identifier_ = identifier;
    type.hasStaticSize_ = hasStaticSize;
    type.numberOfBits_ = numberOfBits;
    return type;
}

Type Type::fromList(Type subtype)
{
    auto type = Type{};
    type.typeEnum_ = TypeEnum::list;
    type.subtype_ = std::make_unique<Type>(std::move(subtype));
    type.hasStaticSize_ = false;
    type.numberOfBits_ = 0;
    return type;
}

Type::Type()
    : typeEnum_{TypeEnum::int32},
      hasStaticSize_{true},
      numberOfBits_{dansandu::farseer::internal::protocol::getNumberOfBits(TypeEnum::int32)}
{
}

Type::Type(const Type& other)
    : typeEnum_{other.typeEnum_},
      identifier_{other.identifier_},
      subtype_{other.subtype_ ? std::make_unique<Type>(*other.subtype_) : nullptr},
      hasStaticSize_{other.hasStaticSize_},
      numberOfBits_{other.numberOfBits_}
{
}

Type::Type(Type&& other) noexcept
    : typeEnum_{other.typeEnum_},
      identifier_{std::move(other.identifier_)},
      subtype_{std::move(other.subtype_)},
      hasStaticSize_{other.hasStaticSize_},
      numberOfBits_{other.numberOfBits_}
{
    other.typeEnum_ = TypeEnum::int32;
    other.identifier_.clear();
    other.hasStaticSize_ = true;
    other.numberOfBits_ = dansandu::farseer::internal::protocol::getNumberOfBits(TypeEnum::int32);
}

Type& Type::operator=(const Type& other)
{
    typeEnum_ = other.typeEnum_;
    identifier_ = other.identifier_;
    subtype_ = other.subtype_ ? std::make_unique<Type>(*other.subtype_) : nullptr;
    hasStaticSize_ = other.hasStaticSize_;
    numberOfBits_ = other.numberOfBits_;

    return *this;
}

Type& Type::operator=(Type&& other) noexcept
{
    if (this != &other)
    {
        typeEnum_ = other.typeEnum_;
        identifier_ = std::move(other.identifier_);
        subtype_ = std::move(other.subtype_);
        hasStaticSize_ = other.hasStaticSize_;
        numberOfBits_ = other.numberOfBits_;

        other.typeEnum_ = TypeEnum::int32;
        other.identifier_.clear();
        other.hasStaticSize_ = true;
        other.numberOfBits_ = dansandu::farseer::internal::protocol::getNumberOfBits(TypeEnum::int32);
    }

    return *this;
}

TypeEnum Type::getTypeEnum() const
{
    return typeEnum_;
}

const std::string& Type::getIdentifier() const
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
        return "std::vector<" + subtype_->getCppType() + ">";
    case TypeEnum::message:
        return identifier_;
    default:
        THROW(std::logic_error, "unrecognized TypeEnum");
    }
}

std::string Type::toString() const
{
    if (typeEnum_ == TypeEnum::list)
    {
        return "list<" + subtype_->toString() + ">";
    }
    else if (typeEnum_ == TypeEnum::message)
    {
        return identifier_;
    }
    else
    {
        return dansandu::farseer::internal::protocol::toString(typeEnum_);
    }
}

uint32_t Type::getHashCode() const
{
    if (typeEnum_ == TypeEnum::list)
    {
        return hashCombine(dansandu::ballotin::hashing::getHashCode32(TypeEnum::list), subtype_->getHashCode());
    }
    else if (typeEnum_ == TypeEnum::message)
    {
        return dansandu::ballotin::hashing::getHashCode32(identifier_);
    }
    else
    {
        return dansandu::ballotin::hashing::getHashCode32(typeEnum_);
    }
}

bool Type::hasStaticSize() const
{
    return hasStaticSize_;
}

uint64_t Type::getNumberOfBits() const
{
    return numberOfBits_;
}

uint32_t Field::getHashCode() const
{
    return hashCombine(type.getHashCode(), dansandu::ballotin::hashing::getHashCode32(identifier));
}

uint32_t MessageProtocol::getHashCode() const
{
    auto hashCode = dansandu::ballotin::hashing::getHashCode32(identifier);

    for (const auto& field : fields)
    {
        hashCode = hashCombine(hashCode, field.getHashCode());
    }

    return hashCode;
}

uint32_t RequestProtocol::getHashCode() const
{
    auto hashCode = dansandu::ballotin::hashing::getHashCode32(identifier);

    for (const auto& field : requestFields)
    {
        hashCode = hashCombine(hashCode, field.getHashCode());
    }

    for (const auto& field : responseFields)
    {
        hashCode = hashCombine(hashCode, field.getHashCode());
    }

    return hashCode;
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
