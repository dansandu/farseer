#include "dansandu/farseer/internal/cpp_protocol.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/ballotin/string.hpp"

#include <format>
#include <sstream>

using dansandu::ballotin::string::join;
using dansandu::ballotin::string::split;
using dansandu::farseer::internal::protocol_definition::FieldDefinition;
using dansandu::farseer::internal::protocol_definition::MessageProtocolDefinition;
using dansandu::farseer::internal::protocol_definition::ProtocolDefinition;
using dansandu::farseer::internal::protocol_definition::RequestProtocolDefinition;

namespace dansandu::farseer::internal::cpp_protocol
{

namespace
{

void generateMessages(const std::vector<MessageProtocolDefinition>& messages, std::ostream& stream)
{
    for (const auto& message : messages)
    {
        // clang-format off
        stream << "struct PRALINE_EXPORT " << message.name << "\n"
               << "{\n"
               << "    struct PRALINE_EXPORT Metadata\n"
               << "    {\n"
               << "        static ::dansandu::farseer::ProtocolIdentifier getProtocolIdentifier();\n"
               << "\n"
               << "        static " << message.name << " deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset);\n"
               << "\n"
               << "        static void serialize(const " << message.name << "& protocol, std::vector<uint8_t>& bytes, size_t& bitsOffset);\n"
               << "\n"
               << "        static constexpr auto hasStaticSize = " << message.hasStaticSize() << ";\n"
               << "\n"
               << "        static constexpr auto staticNumberOfBits = ::dansandu::farseer::ProtocolSize{" << message.getStaticNumberOfBits() << "UL};\n"
               << "    };\n"
               << "\n";
        // clang-format on

        for (const auto& field : message.fields)
        {
            stream << "    " << field.type.getCppType() << " " << field.name << ";\n";
        }

        stream << "};\n\n";
    }
}

void generateRequests(const std::vector<RequestProtocolDefinition>& requests, std::ostream& stream)
{
    for (const auto& request : requests)
    {
        // clang-format off
        stream << "struct PRALINE_EXPORT " << request.name << "\n"
               << "{\n"
               << "    struct PRALINE_EXPORT Metadata\n"
               << "    {\n"
               << "        static ::dansandu::farseer::ProtocolIdentifier getProtocolIdentifier();\n"
               << "\n"
               << "        static " << request.name << " deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset);\n"
               << "\n"
               << "        static void serialize(const " << request.name << "& protocol, std::vector<uint8_t>& bytes, size_t& bitsOffset);\n"
               << "\n"
               << "        static constexpr auto hasStaticSize = " << request.requestHasStaticSize() << ";\n"
               << "\n"
               << "        static constexpr auto staticNumberOfBits = ::dansandu::farseer::ProtocolSize{" << request.getRequestStaticNumberOfBits() << "UL};\n"
               << "    };\n"
               << "\n";
        // clang-format on

        for (const auto& field : request.requestFields)
        {
            stream << "    " << field.type.getCppType() << " " << field.name << ";\n";
        }

        stream << "\n";

        // clang-format off
        stream << "    struct PRALINE_EXPORT Response\n"
               << "    {\n"
               << "        struct PRALINE_EXPORT Metadata\n"
               << "        {\n"
               << "            static ::dansandu::farseer::ProtocolIdentifier getProtocolIdentifier();\n"
               << "\n"
               << "            static Response deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset);\n"
               << "\n"
               << "            static void serialize(const Response& protocol, std::vector<uint8_t>& bytes, size_t& bitsOffset);\n"
               << "\n"
               << "            static constexpr auto hasStaticSize = " << request.responseHasStaticSize() << ";\n"
               << "\n"
               << "            static constexpr auto staticNumberOfBits = ::dansandu::farseer::ProtocolSize{" << request.getResponseStaticNumberOfBits() << "UL};\n"
               << "        };\n"
               << "\n";
        // clang-format on

        for (const auto& field : request.responseFields)
        {
            stream << "        " << field.type.getCppType() << " " << field.name << ";\n";
        }

        stream << "    };\n"
               << "};\n\n";
    }
}

void generateProtocolMetadataDefinition(const std::string& name, const std::vector<FieldDefinition>& fields,
                                        const uint32_t hashCode, std::ostream& stream)
{
    // clang-format off
    stream << "::dansandu::farseer::ProtocolIdentifier " << name << "::Metadata::getProtocolIdentifier()\n"
           << "{\n"
           << "    return ::dansandu::farseer::ProtocolIdentifier{" << hashCode << "U};\n"
           << "}\n"
           << "\n"
           << name << " " << name << "::Metadata::deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)\n"
           << "{\n"
           << "    auto protocol = " << name << "{};\n";
    for (const auto& field : fields)
    {
        stream << "    protocol." << field.name
               << " = ::dansandu::farseer::binary_serialization::BinarySerializer<" << field.type.getCppType()
               << ">::deserialize(bytes, bitsOffset);\n";
    }
    stream << "    return protocol;\n"
           << "}\n"
           << "\n"
           << "void " << name << "::Metadata::serialize(const " << name << "& protocol, std::vector<uint8_t>& bytes, size_t& bitsOffset)\n"
           << "{\n";
    for (const auto& field : fields)
    {
        stream << "    ::dansandu::farseer::binary_serialization::BinarySerializer<" << field.type.getCppType()
               << ">::serialize(protocol." << field.name << ", bytes, bitsOffset);\n";
    }
    stream << "}\n\n";
    // clang-format on
}

}

std::string generateProtocolCppHeader(const ProtocolDefinition& protocol)
{
    auto stream = std::ostringstream{};

    stream << std::boolalpha;

    const auto cppNamespace = join(split(protocol.fileNamespace, "."), "::");

    stream << "#pragma once\n\n"
           << "#include \"dansandu/farseer/common.hpp\"\n\n"
           << "namespace " << cppNamespace << "\n{\n\n";

    generateMessages(protocol.messages, stream);

    generateRequests(protocol.requests, stream);

    stream << "}\n";

    return stream.str();
}

std::string generateProtocolCppSource(const ProtocolDefinition& protocol)
{
    const auto cppInclude = join(split(protocol.fileNamespace, "."), "/");

    const auto cppNamespace = join(split(protocol.fileNamespace, "."), "::");

    auto stream = std::ostringstream{};

    stream << std::boolalpha;

    stream << "#include \"" << cppInclude << ".g.hpp\"\n"
           << "#include \"dansandu/farseer/binary_serialization.hpp\"\n"
           << "#include \"dansandu/farseer/protocol_registry.hpp\"\n"
           << "#include \"dansandu/journey/macro.hpp\"\n\n"
           << "namespace " << cppNamespace << "\n{\n\n";

    for (const auto& message : protocol.messages)
    {
        generateProtocolMetadataDefinition(message.name, message.fields, message.getHashCode(), stream);
    }

    for (const auto& request : protocol.requests)
    {
        generateProtocolMetadataDefinition(request.name, request.requestFields, request.getRequestHashCode(), stream);

        generateProtocolMetadataDefinition(request.name + "::Response", request.responseFields,
                                           request.getResponseHashCode(), stream);
    }

    stream << "}\n\n";

    stream << "namespace\n{\n\n";

    for (const auto& message : protocol.messages)
    {
        stream
            << "const auto DANSANDU_JOURNEY_UNIQUE_NAME(dansandu_farseer_internal_cpp_protocol_registrar) = \n"
            << "    "
               "::dansandu::farseer::protocol_registry::ProtocolRegistry::getGlobalInstance().registerMessageProtocol<"
            << cppNamespace << "::" << message.name << ">();\n\n";
    }

    for (const auto& request : protocol.requests)
    {
        stream
            << "const auto DANSANDU_JOURNEY_UNIQUE_NAME(dansandu_farseer_internal_cpp_protocol_registrar) = \n"
            << "    "
               "::dansandu::farseer::protocol_registry::ProtocolRegistry::getGlobalInstance().registerRequestProtocol<"
            << cppNamespace << "::" << request.name << ">();\n\n";
    }

    stream << "}\n";

    return stream.str();
}

}
