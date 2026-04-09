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

const char* toString(const Type type)
{
    switch (type)
    {
    case Type::i32:
        return "i32";
    case Type::i64:
        return "i64";
    case Type::u32:
        return "u32";
    case Type::u64:
        return "u64";
    case Type::string:
        return "string";
    case Type::boolean:
        return "bool";
    case Type::list:
        return "list";
    case Type::map:
        return "map";
    case Type::message:
        return "message";
    default:
        THROW(std::logic_error, "unrecognized Type");
    }
}

ProtocolSize getStaticNumberOfBits(const Type type)
{
    switch (type)
    {
    case Type::i32:
        return ProtocolSize{32};
    case Type::i64:
        return ProtocolSize{64};
    case Type::u32:
        return ProtocolSize{32};
    case Type::u64:
        return ProtocolSize{64};
    case Type::string:
        THROW(std::logic_error, "string is not a static type");
    case Type::boolean:
        return ProtocolSize{1};
    case Type::list:
        THROW(std::logic_error, "list is not a static type");
    case Type::map:
        THROW(std::logic_error, "map is not a static type");
    case Type::message:
        THROW(std::logic_error, "message is not a static type");
    default:
        THROW(std::logic_error, "unrecognized Type");
    }
}

TypeDefinition TypeDefinition::fromSimple(const Type type)
{
    if (type == Type::list || type == Type::map || type == Type::message)
    {
        THROW(std::logic_error, "this constructor cannot be used for list, map or message types");
    }

    auto typeDefinition = TypeDefinition{};

    typeDefinition.type_ = type;

    if (type == Type::string)
    {
        typeDefinition.hasStaticSize_ = false;
        typeDefinition.staticNumberOfBits_ = ProtocolSize{};
    }
    else
    {
        typeDefinition.hasStaticSize_ = true;
        typeDefinition.staticNumberOfBits_ =
            dansandu::farseer::internal::protocol_definition::getStaticNumberOfBits(type);
    }

    return typeDefinition;
}

TypeDefinition TypeDefinition::fromMessage(const std::string& name, const bool hasStaticSize,
                                           const ProtocolSize staticNumberOfBits)
{
    auto typeDefinition = TypeDefinition{};
    typeDefinition.type_ = Type::message;
    typeDefinition.name_ = name;
    typeDefinition.hasStaticSize_ = hasStaticSize;
    typeDefinition.staticNumberOfBits_ = staticNumberOfBits;
    return typeDefinition;
}

TypeDefinition TypeDefinition::fromList(TypeDefinition subTypeDefinition)
{
    auto typeDefinition = TypeDefinition{};
    typeDefinition.type_ = Type::list;
    typeDefinition.subtypes_.push_back(std::move(subTypeDefinition));
    typeDefinition.hasStaticSize_ = false;
    typeDefinition.staticNumberOfBits_ = ProtocolSize{};
    return typeDefinition;
}

TypeDefinition TypeDefinition::fromMap(TypeDefinition key, TypeDefinition value)
{
    auto typeDefinition = TypeDefinition{};
    typeDefinition.type_ = Type::map;
    typeDefinition.subtypes_.push_back(std::move(key));
    typeDefinition.subtypes_.push_back(std::move(value));
    typeDefinition.hasStaticSize_ = false;
    typeDefinition.staticNumberOfBits_ = ProtocolSize{};
    return typeDefinition;
}

TypeDefinition::TypeDefinition()
    : type_{Type::i32},
      hasStaticSize_{true},
      staticNumberOfBits_{dansandu::farseer::internal::protocol_definition::getStaticNumberOfBits(Type::i32)}
{
}

TypeDefinition::TypeDefinition(const TypeDefinition& other)
    : type_{other.type_},
      name_{other.name_},
      subtypes_{other.subtypes_},
      hasStaticSize_{other.hasStaticSize_},
      staticNumberOfBits_{other.staticNumberOfBits_}
{
}

TypeDefinition::TypeDefinition(TypeDefinition&& other) noexcept
    : type_{other.type_},
      name_{std::move(other.name_)},
      subtypes_{std::move(other.subtypes_)},
      hasStaticSize_{other.hasStaticSize_},
      staticNumberOfBits_{other.staticNumberOfBits_}
{
    other.type_ = Type::i32;
    other.name_.clear();
    other.subtypes_.clear();
    other.hasStaticSize_ = true;
    other.staticNumberOfBits_ = dansandu::farseer::internal::protocol_definition::getStaticNumberOfBits(Type::i32);
}

TypeDefinition& TypeDefinition::operator=(const TypeDefinition& other)
{
    type_ = other.type_;
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
        type_ = other.type_;
        name_ = std::move(other.name_);
        subtypes_ = std::move(other.subtypes_);
        hasStaticSize_ = other.hasStaticSize_;
        staticNumberOfBits_ = other.staticNumberOfBits_;

        other.type_ = Type::i32;
        other.name_.clear();
        other.subtypes_.clear();
        other.hasStaticSize_ = true;
        other.staticNumberOfBits_ = dansandu::farseer::internal::protocol_definition::getStaticNumberOfBits(Type::i32);
    }

    return *this;
}

Type TypeDefinition::getType() const
{
    return type_;
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
    switch (type_)
    {
    case Type::i32:
        return "int32_t";
    case Type::i64:
        return "int64_t";
    case Type::u32:
        return "uint32_t";
    case Type::u64:
        return "uint64_t";
    case Type::string:
        return "std::string";
    case Type::boolean:
        return "bool";
    case Type::list:
        return "std::vector<" + subtypes_.at(0).getCppType() + ">";
    case Type::map:
        return "std::map<" + subtypes_.at(0).getCppType() + ", " + subtypes_.at(1).getCppType() + ">";
    case Type::message:
        return name_;
    default:
        THROW(std::logic_error, "unrecognized Type");
    }
}

std::string TypeDefinition::toString() const
{
    if (type_ == Type::list)
    {
        return "list<" + subtypes_.at(0).toString() + ">";
    }
    else if (type_ == Type::map)
    {
        return "map<" + subtypes_.at(0).toString() + ", " + subtypes_.at(1).toString() + ">";
    }
    else if (type_ == Type::message)
    {
        return name_;
    }
    else
    {
        return dansandu::farseer::internal::protocol_definition::toString(type_);
    }
}

uint32_t TypeDefinition::getHashCode() const
{
    auto hashCode = getHashCode32(type_);

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
    switch (type_)
    {
    case Type::i32:
    case Type::i64:
    case Type::u32:
    case Type::u64:
    case Type::string:
        return true;
    default:
        return false;
    }
}

uint32_t FieldDefinition::getHashCode() const
{
    return hashCombine(typeDefinition.getHashCode(), getHashCode32(name));
}

bool FieldDefinition::hasStaticSize() const
{
    return typeDefinition.hasStaticSize();
}

ProtocolSize FieldDefinition::getStaticNumberOfBits() const
{
    return typeDefinition.getStaticNumberOfBits();
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
    const auto salt = 0x496706CBu;

    return hashCombine(getRequestHashCode(), salt);
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
            stream << "    " << field.typeDefinition.toString() << " " << field.name << ";\n";
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
            stream << "    " << field.typeDefinition.toString() << " " << field.name << ";\n";
        }

        stream << "\n    response\n    {\n";

        for (const auto& field : requestPosition->responseFields)
        {
            stream << "        " << field.typeDefinition.toString() << " " << field.name << ";\n";
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
