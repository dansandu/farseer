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

void generateMessageStructures(const std::vector<MessageProtocol>& messages, std::ostream& stream)
{
    for (const auto& message : messages)
    {
        stream << "struct " << message.identifier << "\n{\n";

        for (const auto& field : message.fields)
        {
            stream << "    " << field.type.getCppType() << " " << field.identifier << ";\n";
        }

        stream << "};\n\n";
    }

    for (const auto& message : messages)
    {
        stream << "PRALINE_EXPORT dansandu::farseer::ProtocolIdentifier get" << message.identifier
               << "ProtocolIdentifier();\n\n";
    }
}

void generateMessageMetadata(const std::vector<MessageProtocol>& messages, const std::string cppNamespace,
                             std::ostream& stream)
{
    for (const auto& message : messages)
    {
        auto deserialization = std::ostringstream{};
        auto serialization = std::ostringstream{};

        for (const auto& field : message.fields)
        {
            const auto fieldCppType = field.type.getCppType();

            deserialization << "        message." << field.identifier
                            << " = dansandu::farseer::binary_serialization::BinarySerializer<" << fieldCppType
                            << ">::deserialize(bytes, bitsOffset);\n";

            serialization << "        dansandu::farseer::binary_serialization::BinarySerializer<" << fieldCppType
                          << ">::serialize(message." << field.identifier << ", bytes, bitsCount);\n";
        }

        stream << std::format(R"(template<>
struct dansandu::farseer::protocol_metadata::ProtocolMetadata<{0}::{1}>
{{
    static dansandu::farseer::ProtocolIdentifier getProtocolIdentifier()
    {{
        return {0}::get{1}ProtocolIdentifier();
    }}

    static constexpr auto hasStaticSize = {3};

    static constexpr auto numberOfBits = uint64_t{{{4}ULL}};
}};

template<>
struct dansandu::farseer::binary_serialization::BinarySerializer<{0}::{1}>
{{
    static {0}::{1} deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {{
        using namespace {0};

        auto message = {1}{{}};
{5}        return message;
    }}

    static void serialize(const {0}::{1}& message, std::vector<uint8_t>& bytes, size_t& bitsCount)
    {{
        using namespace {0};

{6}    }}
}};

)",
                              cppNamespace, message.identifier, message.getHashCode(), message.hasStaticSize,
                              message.numberOfBits, deserialization.str(), serialization.str());
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

    const auto cppNamespace = join(split(protocol.fileNamespace, "."), "::");

    stream << "#pragma once\n\n"
           << "#include \"dansandu/farseer/binary_serialization.hpp\"\n"
           << "#include \"dansandu/farseer/common.hpp\"\n"
           << "#include \"dansandu/farseer/protocol_metadata.hpp\"\n\n"
           << "namespace " << cppNamespace << "\n{\n\n";

    generateMessageStructures(protocol.messages, stream);

    generateRequestStructures(protocol.requests, stream);

    stream << "}\n\n";

    generateMessageMetadata(protocol.messages, cppNamespace, stream);

    return stream.str();
}

std::string generateProtocolCppSource(const Protocol& protocol)
{
    const auto cppInclude = join(split(protocol.fileNamespace, "."), "/");

    const auto cppNamespace = join(split(protocol.fileNamespace, "."), "::");

    auto stream = std::ostringstream{};

    stream << "#include \"" << cppInclude << ".g.hpp\"\n"
           << "#include \"dansandu/farseer/protocol_registry.hpp\"\n"
           << "#include \"dansandu/journey/macro.hpp\"\n\n"
           << "namespace " << cppNamespace << "\n{\n\n";

    for (const auto& message : protocol.messages)
    {
        stream << "dansandu::farseer::ProtocolIdentifier get" << message.identifier
               << "ProtocolIdentifier()\n{\n    return dansandu::farseer::ProtocolIdentifier{" << message.getHashCode()
               << "U};\n}\n\n";
    }

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
