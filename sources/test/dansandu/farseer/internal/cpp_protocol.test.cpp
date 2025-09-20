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
        const auto text = R"(namespace organization.artifact;

message MyMessage
{
    int32 integer;
    bool boolean;
}

)";

        const auto expectedHeader = R"(#pragma once

#include "dansandu/farseer/binary_serialization.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/protocol_metadata.hpp"

namespace organization::artifact
{

struct MyMessage
{
    int32_t integer;
    bool boolean;
};

PRALINE_EXPORT dansandu::farseer::ProtocolIdentifier getMyMessageProtocolIdentifier();

}

template<>
struct dansandu::farseer::protocol_metadata::ProtocolMetadata<organization::artifact::MyMessage>
{
    static dansandu::farseer::ProtocolIdentifier getProtocolIdentifier()
    {
        return organization::artifact::getMyMessageProtocolIdentifier();
    }

    static constexpr auto hasStaticSize = true;

    static constexpr auto numberOfBits = uint64_t{33ULL};
};

template<>
struct dansandu::farseer::binary_serialization::BinarySerializer<organization::artifact::MyMessage>
{
    static organization::artifact::MyMessage deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        using namespace organization::artifact;
        using dansandu::farseer::binary_serialization::BinarySerializer;

        auto message = MyMessage{};
        message.integer = BinarySerializer<int32_t>::deserialize(bytes, bitsOffset);
        message.boolean = BinarySerializer<bool>::deserialize(bytes, bitsOffset);
        return message;
    }

    static void serialize(const organization::artifact::MyMessage& message, std::vector<uint8_t>& bytes, size_t& bitsCount)
    {
        using namespace organization::artifact;
        using dansandu::farseer::binary_serialization::BinarySerializer;

        BinarySerializer<int32_t>::serialize(message.integer, bytes, bitsCount);
        BinarySerializer<bool>::serialize(message.boolean, bytes, bitsCount);
    }
};

)";

        const auto expectedSource = R"(#include "organization/artifact.hpp"
#include "dansandu/farseer/protocol_registry.hpp"
#include "dansandu/journey/macro.hpp"

namespace organization::artifact
{

dansandu::farseer::ProtocolIdentifier getMyMessageProtocolIdentifier()
{
    return dansandu::farseer::ProtocolIdentifier{477867811U};
}

namespace
{

const auto DANSANDU_JOURNEY_UNIQUE_NAME(dansandu_farseer_internal_cpp_protocol_registrar) = 
    dansandu::farseer::protocol_registry::ProtocolRegistry::getGlobalInstance().registerProtocol<MyMessage>();

}

}
)";

        const auto protocol = parseProtocol(text);

        const auto header = generateProtocolCppHeader(protocol);

        const auto source = generateProtocolCppSource(protocol);

        REQUIRE(header == expectedHeader);

        REQUIRE(source == expectedSource);
    }
}
