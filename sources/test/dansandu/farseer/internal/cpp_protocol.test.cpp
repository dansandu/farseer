#include "dansandu/farseer/internal/cpp_protocol.hpp"
#include "dansandu/farseer/internal/protocol_parsing.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::internal::cpp_protocol::generateProtocolCppHeader;
using dansandu::farseer::internal::cpp_protocol::generateProtocolCppSource;
using dansandu::farseer::internal::protocol_parsing::parseProtocol;

TEST_CASE("cpp_protocol")
{
    SECTION("message protocol")
    {
        const auto text = R"(namespace organization.artifact.protocol;

message MyMessage
{
    int32 integer;
    bool boolean;
}
)";

        const auto expectedHeader = R"(#pragma once

#include "dansandu/farseer/binary_serialization.hpp"
#include "dansandu/farseer/common.hpp"

namespace organization::artifact::protocol
{

struct PRALINE_EXPORT MyMessage
{
    struct PRALINE_EXPORT Metadata
    {
        static dansandu::farseer::ProtocolIdentifier getProtocolIdentifier();

        static MyMessage deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static void serialize(const MyMessage& message, std::vector<uint8_t>& bytes, size_t& bitsCount);

        static constexpr auto hasStaticSize = true;

        static constexpr auto numberOfBits = uint64_t{33ULL};
    };

    int32_t integer;
    bool boolean;
};

}
)";

        const auto expectedSource = R"(#include "organization/artifact/protocol.g.hpp"
#include "dansandu/farseer/protocol_registry.hpp"
#include "dansandu/journey/macro.hpp"

namespace organization::artifact::protocol
{

dansandu::farseer::ProtocolIdentifier MyMessage::Metadata::getProtocolIdentifier()
{
    return dansandu::farseer::ProtocolIdentifier{477867811U};
}

MyMessage MyMessage::Metadata::deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    auto message = MyMessage{};
    message.integer = dansandu::farseer::binary_serialization::BinarySerializer<int32_t>::deserialize(bytes, bitsOffset);
    message.boolean = dansandu::farseer::binary_serialization::BinarySerializer<bool>::deserialize(bytes, bitsOffset);
    return message;
}

void MyMessage::Metadata::serialize(const MyMessage& message, std::vector<uint8_t>& bytes, size_t& bitsCount)
{
    dansandu::farseer::binary_serialization::BinarySerializer<int32_t>::serialize(message.integer, bytes, bitsCount);
    dansandu::farseer::binary_serialization::BinarySerializer<bool>::serialize(message.boolean, bytes, bitsCount);
}

}

namespace
{

const auto DANSANDU_JOURNEY_UNIQUE_NAME(dansandu_farseer_internal_cpp_protocol_registrar) = 
    dansandu::farseer::protocol_registry::ProtocolRegistry::getGlobalInstance().registerProtocol<organization::artifact::protocol::MyMessage>();

}
)";

        const auto protocol = parseProtocol(text);

        const auto header = generateProtocolCppHeader(protocol);

        const auto source = generateProtocolCppSource(protocol);

        REQUIRE(header == expectedHeader);

        REQUIRE(source == expectedSource);
    }
}
