#include "dansandu/farseer/internal/cpp_protocol.hpp"
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

// clang-format off
constexpr auto messageClassTemplate = 
R"(struct PRALINE_EXPORT {0}
{{
    struct PRALINE_EXPORT Metadata
    {{
        static ::dansandu::farseer::ProtocolIdentifier getProtocolIdentifier();

        static void serializeHeaderless(const {0}& message, std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static {0} deserializeHeaderless(const std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static std::vector<uint8_t> serializeWithHeader(const {0}& message);

        static bool tryDeserializeWithHeader(const std::vector<uint8_t>& bytes, size_t& bitsOffset, std::any& message);

        static constexpr auto hasStaticSize = {1};

        static constexpr auto staticNumberOfBits = ::dansandu::farseer::ProtocolSize{{{2}UL}};
    }};

)";

constexpr auto requestClassTemplate =
R"(struct PRALINE_EXPORT {0}
{{
    struct PRALINE_EXPORT Metadata
    {{
        static ::dansandu::farseer::ProtocolIdentifier getProtocolIdentifier();

        static void serializeHeaderless(const {0}& request, std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static {0} deserializeHeaderless(const std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static std::vector<uint8_t> serializeWithHeader(const {0}& request, const ::dansandu::farseer::ProtocolSequenceNumber sequenceNumber);

        static bool tryDeserializeWithHeader(const std::vector<uint8_t>& bytes, size_t& bitsOffset, ::dansandu::farseer::ProtocolSequenceNumber& sequenceNumber, std::any& request);

        static constexpr auto hasStaticSize = {1};

        static constexpr auto staticNumberOfBits = ::dansandu::farseer::ProtocolSize{{{2}UL}};
    }};

)";

constexpr auto responseClassTemplate =
R"(    struct PRALINE_EXPORT Response
    {{
        struct PRALINE_EXPORT Metadata
        {{
            static ::dansandu::farseer::ProtocolIdentifier getProtocolIdentifier();

            static void serializeHeaderless(const Response& response, std::vector<uint8_t>& bytes, size_t& bitsOffset);

            static Response deserializeHeaderless(const std::vector<uint8_t>& bytes, size_t& bitsOffset);

            static std::vector<uint8_t> serializeWithHeader(const std::any& response, const ::dansandu::farseer::ProtocolSequenceNumber sequenceNumber);

            static bool tryDeserializeWithHeader(const std::vector<uint8_t>& bytes, size_t& bitsOffset, ::dansandu::farseer::ProtocolSequenceNumber& sequenceNumber, std::any& response);

            static std::any invokeCallback(const UniqueFunction<Response({0}&&)>& callback, std::any&& request);

            static constexpr auto hasStaticSize = {1};

            static constexpr auto staticNumberOfBits = ::dansandu::farseer::ProtocolSize{{{2}UL}};
        }};

)";

constexpr auto protocolIdentifierTemplate =
R"(::dansandu::farseer::ProtocolIdentifier {0}::Metadata::getProtocolIdentifier()
{{
    return ::dansandu::farseer::ProtocolIdentifier{{{1}U}};
}}

)";

constexpr auto messageSerializationWithHeaderTemplate =
R"(std::vector<uint8_t> {0}::Metadata::serializeWithHeader(const {0}& message)
{{
    using ::dansandu::farseer::binary_serialization::BinarySerializer;
    using ::dansandu::farseer::ProtocolIdentifier;

    auto bytes = std::vector<uint8_t>{{}};
    auto bitsOffset = size_t{{0}};

    BinarySerializer<ProtocolIdentifier>::serialize({0}::Metadata::getProtocolIdentifier(), bytes, bitsOffset);

    if constexpr ({0}::Metadata::hasStaticSize)
    {{
        BinarySerializer<{0}>::serialize(message, bytes, bitsOffset);
    }}
    else
    {{
        using ::dansandu::ballotin::binary::bitsPerByte;
        using ::dansandu::ballotin::binary::pushBitsMostSignificant;
        using ::dansandu::farseer::getProtocolSizeFromStdSize;
        using ::dansandu::farseer::ProtocolSize;

        auto dynamicBytes = std::vector<uint8_t>{{}};
        auto dynamicBitsOffset = size_t{{0}};

        BinarySerializer<{0}>::serialize(message, dynamicBytes, dynamicBitsOffset);

        BinarySerializer<ProtocolSize>::serialize(getProtocolSizeFromStdSize(dynamicBitsOffset), bytes, bitsOffset);

        for (const auto byte : dynamicBytes)
        {{
            pushBitsMostSignificant(bytes, bitsOffset, byte, bitsPerByte);
        }}
    }}

    return bytes;
}}

)";

constexpr auto messageDeserializationWithHeaderTemplate =
R"(bool {0}::Metadata::tryDeserializeWithHeader(const std::vector<uint8_t>& bytes, size_t& bitsOffset, std::any& message)
{{
    using ::dansandu::ballotin::binary::bitsPerByte;
    using ::dansandu::farseer::binary_serialization::BinarySerializer;

    if constexpr ({0}::Metadata::hasStaticSize)
    {{
        if (bitsPerByte * bytes.size() >= bitsOffset + {0}::Metadata::staticNumberOfBits.getUnderlying())
        {{
            message = BinarySerializer<{0}>::deserialize(bytes, bitsOffset);
            return true;
        }}
    }}
    else
    {{
        if (bitsPerByte * bytes.size() >= bitsOffset + bitsPerByte * sizeof(ProtocolSize))
        {{
            const auto dynamicNumberOfBits = BinarySerializer<ProtocolSize>::deserialize(bytes, bitsOffset);

            if (bitsPerByte * bytes.size() >= bitsOffset + dynamicNumberOfBits.getUnderlying())
            {{
                message = BinarySerializer<{0}>::deserialize(bytes, bitsOffset);
                return true;
            }}
        }}
    }}
    return false;
}}

)";

constexpr auto requestSerializationWithHeaderTemplate =
R"(std::vector<uint8_t> {0}::Metadata::serializeWithHeader(const {0}& request, const ::dansandu::farseer::ProtocolSequenceNumber sequenceNumber)
{{
    using ::dansandu::farseer::binary_serialization::BinarySerializer;
    using ::dansandu::farseer::ProtocolSequenceNumber;

    auto bytes = std::vector<uint8_t>{{}};
    auto bitsOffset = size_t{{0}};

    BinarySerializer<ProtocolIdentifier>::serialize({0}::Metadata::getProtocolIdentifier(), bytes, bitsOffset);

    BinarySerializer<ProtocolSequenceNumber>::serialize(sequenceNumber, bytes, bitsOffset);

    if constexpr ({0}::Metadata::hasStaticSize)
    {{
        BinarySerializer<{0}>::serialize(request, bytes, bitsOffset);
    }}
    else
    {{
        using ::dansandu::ballotin::binary::bitsPerByte;
        using ::dansandu::ballotin::binary::pushBitsMostSignificant;
        using ::dansandu::farseer::getProtocolSizeFromStdSize;
        using ::dansandu::farseer::ProtocolSize;

        auto dynamicBytes = std::vector<uint8_t>{{}};
        auto dynamicBitsOffset = size_t{{0}};

        BinarySerializer<{0}>::serialize(request, dynamicBytes, dynamicBitsOffset);

        BinarySerializer<ProtocolSize>::serialize(getProtocolSizeFromStdSize(dynamicBitsOffset), bytes, bitsOffset);

        for (const auto byte : dynamicBytes)
        {{
            pushBitsMostSignificant(bytes, bitsOffset, byte, bitsPerByte);
        }}
    }}

    return bytes;
}}

)";

constexpr auto requestDeserializationWithHeaderTemplate =
R"(bool {0}::Metadata::tryDeserializeWithHeader(const std::vector<uint8_t>& bytes, size_t& bitsOffset, ::dansandu::farseer::ProtocolSequenceNumber& sequenceNumber, std::any& request)
{{
    using ::dansandu::ballotin::binary::bitsPerByte;
    using ::dansandu::farseer::binary_serialization::BinarySerializer;

    if constexpr ({0}::Metadata::hasStaticSize)
    {{
        if (bitsPerByte * bytes.size() >= bitsOffset + bitsPerByte * sizeof(ProtocolSequenceNumber) +
                                              {0}::Metadata::staticNumberOfBits.getUnderlying())
        {{
            sequenceNumber = BinarySerializer<ProtocolSequenceNumber>::deserialize(bytes, bitsOffset);
            request = BinarySerializer<{0}>::deserialize(bytes, bitsOffset);
            return true;
        }}
    }}
    else
    {{
        if (bitsPerByte * bytes.size() >=
            bitsOffset + bitsPerByte * sizeof(ProtocolSequenceNumber) + bitsPerByte * sizeof(ProtocolSize))
        {{
            sequenceNumber = BinarySerializer<ProtocolSequenceNumber>::deserialize(bytes, bitsOffset);

            const auto dynamicNumberOfBits = BinarySerializer<ProtocolSize>::deserialize(bytes, bitsOffset);

            if (bitsPerByte * bytes.size() >= bitsOffset + dynamicNumberOfBits.getUnderlying())
            {{
                request = BinarySerializer<{0}>::deserialize(bytes, bitsOffset);
                return true;
            }}
        }}
    }}
    return false;
}}

)";

constexpr auto responseSerializationWithHeaderTemplate =
R"(std::vector<uint8_t> {0}::Response::Metadata::serializeWithHeader(const std::any& response, const ::dansandu::farseer::ProtocolSequenceNumber sequenceNumber)
{{
    using ::dansandu::farseer::binary_serialization::BinarySerializer;
    using ::dansandu::ballotin::binary::bitsPerByte;
    using ::dansandu::ballotin::binary::pushBitsMostSignificant;
    using ::dansandu::farseer::getProtocolSizeFromStdSize;
    using ::dansandu::farseer::ProtocolSize;
    using ::dansandu::farseer::ProtocolIdentifier;
    using ::dansandu::farseer::ProtocolSequenceNumber;
    using ::dansandu::farseer::Expected;

    const auto& casted = std::any_cast<const Expected<{0}::Response>&>(response);

    auto bytes = std::vector<uint8_t>{{}};
    auto bitsOffset = size_t{{0}};

    BinarySerializer<ProtocolIdentifier>::serialize({0}::Response::Metadata::getProtocolIdentifier(), bytes, bitsOffset);

    BinarySerializer<ProtocolSequenceNumber>::serialize(sequenceNumber, bytes, bitsOffset);

    auto dynamicBytes = std::vector<uint8_t>{{}};
    auto dynamicBitsOffset = size_t{{0}};

    BinarySerializer<Expected<{0}::Response>>::serialize(casted, dynamicBytes, dynamicBitsOffset);

    BinarySerializer<ProtocolSize>::serialize(getProtocolSizeFromStdSize(dynamicBitsOffset), bytes, bitsOffset);

    for (const auto byte : dynamicBytes)
    {{
        pushBitsMostSignificant(bytes, bitsOffset, byte, bitsPerByte);
    }}

    return bytes;
}}

)";

constexpr auto responseDeserializationWithHeaderTemplate =
R"(bool {0}::Response::Metadata::tryDeserializeWithHeader(const std::vector<uint8_t>& bytes, size_t& bitsOffset, ::dansandu::farseer::ProtocolSequenceNumber& sequenceNumber, std::any& response)
{{
    using ::dansandu::ballotin::binary::bitsPerByte;
    using ::dansandu::farseer::binary_serialization::BinarySerializer;
    using ::dansandu::farseer::ProtocolSize;
    using ::dansandu::farseer::ProtocolSequenceNumber;
    using ::dansandu::farseer::Expected;

    if (bitsPerByte * bytes.size() >=
        bitsOffset + bitsPerByte * sizeof(ProtocolSequenceNumber) + bitsPerByte * sizeof(ProtocolSize))
    {{
        sequenceNumber = BinarySerializer<ProtocolSequenceNumber>::deserialize(bytes, bitsOffset);

        const auto dynamicNumberOfBits = BinarySerializer<ProtocolSize>::deserialize(bytes, bitsOffset);

        if (bitsPerByte * bytes.size() >= bitsOffset + dynamicNumberOfBits.getUnderlying())
        {{
            response = BinarySerializer<Expected<{0}::Response>>::deserialize(bytes, bitsOffset);
            return true;
        }}
    }}

    return false;
}}

)";

constexpr auto invokeCallbackTemplate =
R"(std::any {0}::Response::Metadata::invokeCallback(const UniqueFunction<Response({0}&&)>& callback, std::any&& request)
{{
    using ::dansandu::farseer::Expected;

    try
    {{
        return Expected<Response>::fromSuccess(callback(std::any_cast<{0}&&>(std::move(request))));
    }}
    catch (const RequestProtocolError& exception)
    {{
        return Expected<Response>::fromFailure(exception.getErrorCode(), exception.getErrorMessage());
    }}
    catch (const std::exception& exception)
    {{
        return Expected<Response>::fromInternalServerError(exception.what());
    }}
    catch (...)
    {{
        return Expected<Response>::fromInternalServerError();
    }}
}}

)";
// clang-format on

void generateMessages(const std::vector<MessageProtocolDefinition>& messages, std::ostream& stream)
{
    for (const auto& message : messages)
    {
        stream << std::format(messageClassTemplate, message.name, message.hasStaticSize(),
                              message.getStaticNumberOfBits().getUnderlying());

        for (const auto& field : message.fields)
        {
            stream << "    " << field.typeDefinition.getCppType() << " " << field.name << ";\n";
        }

        stream << "};\n\n";
    }
}

void generateRequests(const std::vector<RequestProtocolDefinition>& requests, std::ostream& stream)
{
    for (const auto& request : requests)
    {
        stream << std::format(requestClassTemplate, request.name, request.requestHasStaticSize(),
                              request.getRequestStaticNumberOfBits().getUnderlying());

        for (const auto& field : request.requestFields)
        {
            stream << "    " << field.typeDefinition.getCppType() << " " << field.name << ";\n";
        }

        stream << "\n";

        stream << std::format(responseClassTemplate, request.name, request.responseHasStaticSize(),
                              request.getResponseStaticNumberOfBits().getUnderlying());

        for (const auto& field : request.responseFields)
        {
            stream << "        " << field.typeDefinition.getCppType() << " " << field.name << ";\n";
        }

        stream << "    };\n"
               << "};\n\n";
    }
}

void generateMessageMetadataDefinition(const MessageProtocolDefinition& message, std::ostream& stream)
{
    stream << std::format(protocolIdentifierTemplate, message.name, message.getHashCode());

    if (message.fields.empty())
    {
        stream << "void " << message.name << "::Metadata::serializeHeaderless(const " << message.name
               << "&, std::vector<uint8_t>&, size_t&)\n";
    }
    else
    {
        stream << "void " << message.name << "::Metadata::serializeHeaderless(const " << message.name
               << "& message, std::vector<uint8_t>& bytes, size_t& bitsOffset)\n";
    }

    stream << "{\n";

    for (const auto& field : message.fields)
    {
        stream << "    ::dansandu::farseer::binary_serialization::BinarySerializer<"
               << field.typeDefinition.getCppType() << ">::serialize(message." << field.name
               << ", bytes, bitsOffset);\n";
    }

    stream << "}\n\n";

    if (message.fields.empty())
    {
        stream << message.name << " " << message.name
               << "::Metadata::deserializeHeaderless(const std::vector<uint8_t>&, size_t&)\n";
    }
    else
    {
        stream << message.name << " " << message.name
               << "::Metadata::deserializeHeaderless(const std::vector<uint8_t>& bytes, size_t& bitsOffset)\n";
    }

    stream << "{\n"
           << "    auto message = " << message.name << "{};\n";

    for (const auto& field : message.fields)
    {
        stream << "    message." << field.name << " = ::dansandu::farseer::binary_serialization::BinarySerializer<"
               << field.typeDefinition.getCppType() << ">::deserialize(bytes, bitsOffset);\n";
    }

    stream << "    return message;\n"
           << "}\n\n";

    stream << std::format(messageSerializationWithHeaderTemplate, message.name)
           << std::format(messageDeserializationWithHeaderTemplate, message.name);
}

void generateRequestMetadataDefinition(const RequestProtocolDefinition& request, std::ostream& stream)
{
    stream << std::format(protocolIdentifierTemplate, request.name, request.getRequestHashCode());

    if (request.requestFields.empty())
    {
        stream << "void " << request.name << "::Metadata::serializeHeaderless(const " << request.name
               << "&, std::vector<uint8_t>&, size_t&)\n";
    }
    else
    {
        stream << "void " << request.name << "::Metadata::serializeHeaderless(const " << request.name
               << "& request, std::vector<uint8_t>& bytes, size_t& bitsOffset)\n";
    }

    stream << "{\n";

    for (const auto& field : request.requestFields)
    {
        stream << "    ::dansandu::farseer::binary_serialization::BinarySerializer<"
               << field.typeDefinition.getCppType() << ">::serialize(request." << field.name
               << ", bytes, bitsOffset);\n";
    }

    stream << "}\n\n";

    if (request.requestFields.empty())
    {
        stream << request.name << " " << request.name
               << "::Metadata::deserializeHeaderless(const std::vector<uint8_t>&, size_t&)\n";
    }
    else
    {
        stream << request.name << " " << request.name
               << "::Metadata::deserializeHeaderless(const std::vector<uint8_t>& bytes, size_t& bitsOffset)\n";
    }

    stream << "{\n"
           << "    auto request = " << request.name << "{};\n";

    for (const auto& field : request.requestFields)
    {
        stream << "    request." << field.name << " = ::dansandu::farseer::binary_serialization::BinarySerializer<"
               << field.typeDefinition.getCppType() << ">::deserialize(bytes, bitsOffset);\n";
    }

    stream << "    return request;\n"
           << "}\n\n";

    stream << std::format(requestSerializationWithHeaderTemplate, request.name)
           << std::format(requestDeserializationWithHeaderTemplate, request.name);
}

void generateResponseMetadataDefinition(const RequestProtocolDefinition& request, std::ostream& stream)
{
    stream << std::format(protocolIdentifierTemplate, request.name + "::Response", request.getResponseHashCode());

    if (request.responseFields.empty())
    {
        stream << "void " << request.name << "::Response::Metadata::serializeHeaderless(const " << request.name
               << "::Response&, std::vector<uint8_t>&, size_t&)\n";
    }
    else
    {
        stream << "void " << request.name << "::Response::Metadata::serializeHeaderless(const " << request.name
               << "::Response& response, std::vector<uint8_t>& bytes, size_t& bitsOffset)\n";
    }

    stream << "{\n";

    for (const auto& field : request.responseFields)
    {
        stream << "    ::dansandu::farseer::binary_serialization::BinarySerializer<"
               << field.typeDefinition.getCppType() << ">::serialize(response." << field.name
               << ", bytes, bitsOffset);\n";
    }

    stream << "}\n\n";

    if (request.responseFields.empty())
    {
        stream << request.name << "::Response " << request.name
               << "::Response::Metadata::deserializeHeaderless(const std::vector<uint8_t>&, size_t&)\n";
    }
    else
    {
        stream
            << request.name << "::Response " << request.name
            << "::Response::Metadata::deserializeHeaderless(const std::vector<uint8_t>& bytes, size_t& bitsOffset)\n";
    }

    stream << "{\n"
           << "    auto response = " << request.name << "::Response{};\n";

    for (const auto& field : request.responseFields)
    {
        stream << "    response." << field.name << " = ::dansandu::farseer::binary_serialization::BinarySerializer<"
               << field.typeDefinition.getCppType() << ">::deserialize(bytes, bitsOffset);\n";
    }

    stream << "    return response;\n"
           << "}\n\n";

    stream << std::format(responseSerializationWithHeaderTemplate, request.name)
           << std::format(responseDeserializationWithHeaderTemplate, request.name)
           << std::format(invokeCallbackTemplate, request.name);
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
           << "#include \"dansandu/ballotin/binary.hpp\"\n"
           << "#include \"dansandu/farseer/binary_serialization.hpp\"\n"
           << "#include \"dansandu/farseer/protocol_registry.hpp\"\n"
           << "#include \"dansandu/journey/macro.hpp\"\n\n"
           << "namespace " << cppNamespace << "\n{\n\n";

    for (const auto& message : protocol.messages)
    {
        generateMessageMetadataDefinition(message, stream);
    }

    for (const auto& request : protocol.requests)
    {
        generateRequestMetadataDefinition(request, stream);

        generateResponseMetadataDefinition(request, stream);
    }

    stream << "}\n\n";

    stream << "namespace\n{\n\n";

    for (const auto& message : protocol.messages)
    {
        stream
            << "const auto DANSANDU_JOURNEY_UNIQUE_NAME =\n"
            << "    "
               "::dansandu::farseer::protocol_registry::ProtocolRegistry::getGlobalInstance().registerMessageProtocol<"
            << cppNamespace << "::" << message.name << ">();\n\n";
    }

    for (const auto& request : protocol.requests)
    {
        stream
            << "const auto DANSANDU_JOURNEY_UNIQUE_NAME =\n"
            << "    "
               "::dansandu::farseer::protocol_registry::ProtocolRegistry::getGlobalInstance().registerRequestProtocol<"
            << cppNamespace << "::" << request.name << ">();\n\n";
    }

    stream << "}\n";

    return stream.str();
}

}
