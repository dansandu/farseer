#include "dansandu/farseer/internal/cpp_protocol.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/ballotin/string.hpp"

#include <format>
#include <sstream>

using dansandu::ballotin::string::join;
using dansandu::ballotin::string::split;
using dansandu::farseer::internal::protocol::MessageProtocol;
using dansandu::farseer::internal::protocol::Protocol;
using dansandu::farseer::internal::protocol::RequestProtocol;

namespace dansandu::farseer::internal::cpp_protocol
{

namespace
{

void generateMessages(const std::vector<MessageProtocol>& messages, std::ostream& stream)
{
    for (const auto& message : messages)
    {
        // clang-format off
        stream 
        << "struct PRALINE_EXPORT " << message.identifier << "\n"
        << "{\n"
        << "    struct PRALINE_EXPORT Metadata\n"
        << "    {\n"
        << "        static dansandu::farseer::ProtocolIdentifier getProtocolIdentifier();\n"
        << "\n"
        << "        static " << message.identifier << " deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset);\n"
        << "\n"
        << "        static void serialize(const " << message.identifier << "& message, std::vector<uint8_t>& bytes, size_t& bitsCount);\n"
        << "\n"
        << "        static constexpr auto hasStaticSize = " << message.hasStaticSize << ";\n"
        << "\n"
        << "        static constexpr auto numberOfBits = uint64_t{" << message.numberOfBits << "ULL};\n"
        << "    };\n"
        << "\n";
        // clang-format on

        for (const auto& field : message.fields)
        {
            stream << "    " << field.type.getCppType() << " " << field.identifier << ";\n";
        }

        stream << "};\n\n";
    }
}

void generateMessagesMetadataDefinitions(const std::vector<MessageProtocol>& messages, std::ostream& stream)
{
    for (const auto& message : messages)
    {
        // clang-format off
        stream << "dansandu::farseer::ProtocolIdentifier " << message.identifier << "::Metadata::getProtocolIdentifier()\n"
               << "{\n"
               << "    return dansandu::farseer::ProtocolIdentifier{" << message.getHashCode() << "U};\n"
               << "}\n"
               << "\n"
               << message.identifier << " " << message.identifier << "::Metadata::deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)\n"
               << "{\n"
               << "    auto message = " << message.identifier << "{};\n";
        for (const auto& field : message.fields)
        {
            stream << "    message." << field.identifier
                   << " = dansandu::farseer::binary_serialization::BinarySerializer<" << field.type.getCppType()
                   << ">::deserialize(bytes, bitsOffset);\n";
        }
        stream << "    return message;\n"
               << "}\n"
               << "\n"
               << "void " << message.identifier << "::Metadata::serialize(const " << message.identifier << "& message, std::vector<uint8_t>& bytes, size_t& bitsCount)\n"
               << "{\n";
        for (const auto& field : message.fields)
        {
            stream << "    dansandu::farseer::binary_serialization::BinarySerializer<" << field.type.getCppType()
                   << ">::serialize(message." << field.identifier << ", bytes, bitsCount);\n";
        }
        stream << "}\n\n";
        // clang-format on
    }
}

void generateRequestStructures(const std::vector<RequestProtocol>& requests, std::ostream& stream)
{
    for (const auto& request : requests)
    {
        stream << "struct PRALINE_EXPORT " << request.identifier << "\n{\n"
               << "    static constexpr uint32_t identifier = " << request.getHashCode() << "u;\n\n"
               << "    void deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset);\n\n"
               << "    void serialize(std::vector<uint8_t>& bytes, size_t& bitsCount) const;\n\n";

        for (const auto& field : request.requestFields)
        {
            stream << "    " << field.type.getCppType() << " " << field.identifier << ";\n";
        }

        stream << "\n    struct PRALINE_EXPORT Response\n    {\n"
               << "        void deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset);\n\n"
               << "        void serialize(std::vector<uint8_t>& bytes, size_t& bitsCount) const;\n\n";

        for (const auto& field : request.responseFields)
        {
            stream << "        " << field.type.getCppType() << " " << field.identifier << ";\n";
        }

        stream << "    };\n};\n\n";
    }
}

}

std::string generateProtocolCppHeader(const Protocol& protocol)
{
    auto stream = std::ostringstream{};

    stream << std::boolalpha;

    const auto cppNamespace = join(split(protocol.fileNamespace, "."), "::");

    stream << "#pragma once\n\n"
           << "#include \"dansandu/farseer/binary_serialization.hpp\"\n"
           << "#include \"dansandu/farseer/common.hpp\"\n\n"
           << "namespace " << cppNamespace << "\n{\n\n";

    generateMessages(protocol.messages, stream);

    generateRequestStructures(protocol.requests, stream);

    stream << "}\n";

    return stream.str();
}

std::string generateProtocolCppSource(const Protocol& protocol)
{
    const auto cppInclude = join(split(protocol.fileNamespace, "."), "/");

    const auto cppNamespace = join(split(protocol.fileNamespace, "."), "::");

    auto stream = std::ostringstream{};

    stream << std::boolalpha;

    stream << "#include \"" << cppInclude << ".g.hpp\"\n"
           << "#include \"dansandu/farseer/protocol_registry.hpp\"\n"
           << "#include \"dansandu/journey/macro.hpp\"\n\n"
           << "namespace " << cppNamespace << "\n{\n\n";

    generateMessagesMetadataDefinitions(protocol.messages, stream);

    stream << "}\n\n";

    stream << "namespace\n{\n\n";

    for (const auto& message : protocol.messages)
    {
        stream << "const auto DANSANDU_JOURNEY_UNIQUE_NAME(dansandu_farseer_internal_cpp_protocol_registrar) = \n"
               << "    dansandu::farseer::protocol_registry::ProtocolRegistry::getGlobalInstance().registerProtocol<"
               << cppNamespace << "::" << message.identifier << ">();\n\n";
    }

    stream << "}\n";

    return stream.str();
}

}
