#include "dansandu/farseer/internal/protocol_definition.hpp"
#include "dansandu/ballotin/exception.hpp"

#include <sstream>

namespace dansandu::farseer::internal::protocol_definition
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

std::string Type::toString() const
{
    if (typeEnum == TypeEnum::custom)
    {
        return identifier;
    }
    else if (typeEnum == TypeEnum::list)
    {
        if (!subtype)
        {
            THROW(std::logic_error, "subtype cannot be nullptr when type is list");
        }
        return "list<" + subtype->toString() + ">";
    }
    else
    {
        return dansandu::farseer::internal::protocol_definition::toString(typeEnum);
    }
}

std::string ProtocolFile::toString() const
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
