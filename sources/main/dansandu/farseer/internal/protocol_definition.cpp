#include "dansandu/farseer/internal/protocol_definition.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/ballotin/hashing.hpp"

#include <algorithm>
#include <numeric>
#include <sstream>

using dansandu::ballotin::hashing::getHashCode32;
using dansandu::ballotin::hashing::hashCombine;

namespace dansandu::farseer::internal::protocol_definition
{

const char* toString(const TypeDefinitionEnum typeEnum)
{
    switch (typeEnum)
    {
    case TypeDefinitionEnum::int32:
        return "int32";
    case TypeDefinitionEnum::int64:
        return "int64";
    case TypeDefinitionEnum::uint32:
        return "uint32";
    case TypeDefinitionEnum::uint64:
        return "uint64";
    case TypeDefinitionEnum::string:
        return "string";
    case TypeDefinitionEnum::boolean:
        return "bool";
    case TypeDefinitionEnum::list:
        return "list";
    case TypeDefinitionEnum::map:
        return "map";
    case TypeDefinitionEnum::message:
        return "message";
    default:
        THROW(std::logic_error, "unrecognized TypeDefinitionEnum");
    }
}

ProtocolSize getStaticNumberOfBits(const TypeDefinitionEnum typeEnum)
{
    switch (typeEnum)
    {
    case TypeDefinitionEnum::int32:
        return ProtocolSize{32};
    case TypeDefinitionEnum::int64:
        return ProtocolSize{64};
    case TypeDefinitionEnum::uint32:
        return ProtocolSize{32};
    case TypeDefinitionEnum::uint64:
        return ProtocolSize{64};
    case TypeDefinitionEnum::string:
        THROW(std::logic_error, "string is not a static type");
    case TypeDefinitionEnum::boolean:
        return ProtocolSize{1};
    case TypeDefinitionEnum::list:
        THROW(std::logic_error, "list is not a static type");
    case TypeDefinitionEnum::map:
        THROW(std::logic_error, "map is not a static type");
    case TypeDefinitionEnum::message:
        THROW(std::logic_error, "message is not a static type");
    default:
        THROW(std::logic_error, "unrecognized TypeDefinitionEnum");
    }
}

TypeDefinition TypeDefinition::fromSimple(const TypeDefinitionEnum typeEnum)
{
    if (typeEnum == TypeDefinitionEnum::list || typeEnum == TypeDefinitionEnum::map ||
        typeEnum == TypeDefinitionEnum::message)
    {
        THROW(std::logic_error, "this constructor cannot be used for list, map or message types");
    }

    auto type = TypeDefinition{};
    type.typeEnum_ = typeEnum;

    if (typeEnum == TypeDefinitionEnum::string)
    {
        type.hasStaticSize_ = false;
        type.staticNumberOfBits_ = ProtocolSize{};
    }
    else
    {
        type.hasStaticSize_ = true;
        type.staticNumberOfBits_ = dansandu::farseer::internal::protocol_definition::getStaticNumberOfBits(typeEnum);
    }

    return type;
}

TypeDefinition TypeDefinition::fromMessage(const std::string& name, const bool hasStaticSize,
                                           const ProtocolSize staticNumberOfBits)
{
    auto type = TypeDefinition{};
    type.typeEnum_ = TypeDefinitionEnum::message;
    type.name_ = name;
    type.hasStaticSize_ = hasStaticSize;
    type.staticNumberOfBits_ = staticNumberOfBits;
    return type;
}

TypeDefinition TypeDefinition::fromList(TypeDefinition subtype)
{
    auto type = TypeDefinition{};
    type.typeEnum_ = TypeDefinitionEnum::list;
    type.subtypes_.push_back(std::move(subtype));
    type.hasStaticSize_ = false;
    type.staticNumberOfBits_ = ProtocolSize{};
    return type;
}

TypeDefinition TypeDefinition::fromMap(TypeDefinition key, TypeDefinition value)
{
    auto type = TypeDefinition{};
    type.typeEnum_ = TypeDefinitionEnum::map;
    type.subtypes_.push_back(std::move(key));
    type.subtypes_.push_back(std::move(value));
    type.hasStaticSize_ = false;
    type.staticNumberOfBits_ = ProtocolSize{};
    return type;
}

TypeDefinition::TypeDefinition()
    : typeEnum_{TypeDefinitionEnum::int32},
      hasStaticSize_{true},
      staticNumberOfBits_{
          dansandu::farseer::internal::protocol_definition::getStaticNumberOfBits(TypeDefinitionEnum::int32)}
{
}

TypeDefinition::TypeDefinition(const TypeDefinition& other)
    : typeEnum_{other.typeEnum_},
      name_{other.name_},
      subtypes_{other.subtypes_},
      hasStaticSize_{other.hasStaticSize_},
      staticNumberOfBits_{other.staticNumberOfBits_}
{
}

TypeDefinition::TypeDefinition(TypeDefinition&& other) noexcept
    : typeEnum_{other.typeEnum_},
      name_{std::move(other.name_)},
      subtypes_{std::move(other.subtypes_)},
      hasStaticSize_{other.hasStaticSize_},
      staticNumberOfBits_{other.staticNumberOfBits_}
{
    other.typeEnum_ = TypeDefinitionEnum::int32;
    other.name_.clear();
    other.subtypes_.clear();
    other.hasStaticSize_ = true;
    other.staticNumberOfBits_ =
        dansandu::farseer::internal::protocol_definition::getStaticNumberOfBits(TypeDefinitionEnum::int32);
}

TypeDefinition& TypeDefinition::operator=(const TypeDefinition& other)
{
    typeEnum_ = other.typeEnum_;
    name_ = other.name_;
    subtypes_ = other.subtypes_;
    hasStaticSize_ = other.hasStaticSize_;
    staticNumberOfBits_ = other.staticNumberOfBits_;

    return *this;
}

TypeDefinition& TypeDefinition::operator=(TypeDefinition&& other) noexcept
{
    if (this != &other)
    {
        typeEnum_ = other.typeEnum_;
        name_ = std::move(other.name_);
        subtypes_ = std::move(other.subtypes_);
        hasStaticSize_ = other.hasStaticSize_;
        staticNumberOfBits_ = other.staticNumberOfBits_;

        other.typeEnum_ = TypeDefinitionEnum::int32;
        other.name_.clear();
        other.subtypes_.clear();
        other.hasStaticSize_ = true;
        other.staticNumberOfBits_ =
            dansandu::farseer::internal::protocol_definition::getStaticNumberOfBits(TypeDefinitionEnum::int32);
    }

    return *this;
}

TypeDefinitionEnum TypeDefinition::getTypeEnum() const
{
    return typeEnum_;
}

const std::string& TypeDefinition::getName() const
{
    return name_;
}

const std::vector<TypeDefinition>& TypeDefinition::getSubtypes() const
{
    return subtypes_;
}

std::string TypeDefinition::getCppType() const
{
    switch (typeEnum_)
    {
    case TypeDefinitionEnum::int32:
        return "int32_t";
    case TypeDefinitionEnum::int64:
        return "int64_t";
    case TypeDefinitionEnum::uint32:
        return "uint32_t";
    case TypeDefinitionEnum::uint64:
        return "uint64_t";
    case TypeDefinitionEnum::string:
        return "std::string";
    case TypeDefinitionEnum::boolean:
        return "bool";
    case TypeDefinitionEnum::list:
        return "std::vector<" + subtypes_.at(0).getCppType() + ">";
    case TypeDefinitionEnum::map:
        return "std::map<" + subtypes_.at(0).getCppType() + ", " + subtypes_.at(1).getCppType() + ">";
    case TypeDefinitionEnum::message:
        return name_;
    default:
        THROW(std::logic_error, "unrecognized TypeDefinitionEnum");
    }
}

std::string TypeDefinition::toString() const
{
    if (typeEnum_ == TypeDefinitionEnum::list)
    {
        return "list<" + subtypes_.at(0).toString() + ">";
    }
    else if (typeEnum_ == TypeDefinitionEnum::map)
    {
        return "map<" + subtypes_.at(0).toString() + ", " + subtypes_.at(1).toString() + ">";
    }
    else if (typeEnum_ == TypeDefinitionEnum::message)
    {
        return name_;
    }
    else
    {
        return dansandu::farseer::internal::protocol_definition::toString(typeEnum_);
    }
}

uint32_t TypeDefinition::getHashCode() const
{
    auto hashCode = getHashCode32(typeEnum_);

    hashCombine(hashCode, getHashCode32(name_));

    for (const auto& subtype : subtypes_)
    {
        hashCombine(hashCode, subtype.getHashCode());
    }

    return hashCode;
}

bool TypeDefinition::hasStaticSize() const
{
    return hasStaticSize_;
}

ProtocolSize TypeDefinition::getStaticNumberOfBits() const
{
    return staticNumberOfBits_;
}

bool TypeDefinition::canBeMapKey() const
{
    switch (typeEnum_)
    {
    case TypeDefinitionEnum::int32:
    case TypeDefinitionEnum::int64:
    case TypeDefinitionEnum::uint32:
    case TypeDefinitionEnum::uint64:
    case TypeDefinitionEnum::string:
        return true;
    default:
        return false;
    }
}

uint32_t FieldDefinition::getHashCode() const
{
    return hashCombine(type.getHashCode(), getHashCode32(name));
}

bool FieldDefinition::hasStaticSize() const
{
    return type.hasStaticSize();
}

ProtocolSize FieldDefinition::getStaticNumberOfBits() const
{
    return type.getStaticNumberOfBits();
}

uint32_t MessageProtocolDefinition::getHashCode() const
{
    auto hashCode = getHashCode32(fileNamespace);

    hashCode = hashCombine(hashCode, getHashCode32(name));

    for (const auto& field : fields)
    {
        hashCode = hashCombine(hashCode, field.getHashCode());
    }

    return hashCode;
}

bool MessageProtocolDefinition::hasStaticSize() const
{
    return std::all_of(fields.cbegin(), fields.cend(), [](const auto& field) { return field.hasStaticSize(); });
}

ProtocolSize MessageProtocolDefinition::getStaticNumberOfBits() const
{
    return std::accumulate(fields.cbegin(), fields.cend(), ProtocolSize{},
                           [](const auto total, const auto& field) { return total + field.getStaticNumberOfBits(); });
}

uint32_t RequestProtocolDefinition::getRequestHashCode() const
{
    auto hashCode = getHashCode32(fileNamespace);

    hashCode = hashCombine(hashCode, getHashCode32(name));

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

uint32_t RequestProtocolDefinition::getResponseHashCode() const
{
    return hashCombine(getRequestHashCode(), 0x496706CBU);
}

bool RequestProtocolDefinition::requestHasStaticSize() const
{
    return std::all_of(requestFields.cbegin(), requestFields.cend(),
                       [](const auto& field) { return field.hasStaticSize(); });
}

ProtocolSize RequestProtocolDefinition::getRequestStaticNumberOfBits() const
{
    return std::accumulate(requestFields.cbegin(), requestFields.cend(), ProtocolSize{},
                           [](const auto total, const auto& field) { return total + field.getStaticNumberOfBits(); });
}

bool RequestProtocolDefinition::responseHasStaticSize() const
{
    return std::all_of(responseFields.cbegin(), responseFields.cend(),
                       [](const auto& field) { return field.hasStaticSize(); });
}

ProtocolSize RequestProtocolDefinition::getResponseStaticNumberOfBits() const
{
    return std::accumulate(responseFields.cbegin(), responseFields.cend(), ProtocolSize{},
                           [](const auto total, const auto& field) { return total + field.getStaticNumberOfBits(); });
}

std::string ProtocolDefinition::toString() const
{
    auto stream = std::ostringstream{};

    stream << "namespace " << fileNamespace << ";\n\n";

    for (auto messagePosition = messages.cbegin(); messagePosition != messages.cend(); ++messagePosition)
    {
        stream << "message " << messagePosition->name << "\n{\n";

        for (const auto& field : messagePosition->fields)
        {
            stream << "    " << field.type.toString() << " " << field.name << ";\n";
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
        stream << "request " << requestPosition->name << "\n{\n";

        for (const auto& field : requestPosition->requestFields)
        {
            stream << "    " << field.type.toString() << " " << field.name << ";\n";
        }

        stream << "\n    response\n    {\n";

        for (const auto& field : requestPosition->responseFields)
        {
            stream << "        " << field.type.toString() << " " << field.name << ";\n";
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
