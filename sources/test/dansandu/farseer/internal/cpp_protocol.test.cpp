#include "dansandu/farseer/internal/cpp_protocol.hpp"
#include "dansandu/farseer/internal/protocol_definition_parsing.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::internal::cpp_protocol::generateProtocolCppHeader;
using dansandu::farseer::internal::cpp_protocol::generateProtocolCppSource;
using dansandu::farseer::internal::protocol_definition_parsing::parseProtocolDefinition;

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

#include "dansandu/farseer/common.hpp"

namespace organization::artifact::protocol
{

struct PRALINE_EXPORT MyMessage
{
    struct PRALINE_EXPORT Metadata
    {
        static ::dansandu::farseer::ProtocolIdentifier getProtocolIdentifier();

        static MyMessage deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static void serialize(const MyMessage& protocol, std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static constexpr auto hasStaticSize = true;

        static constexpr auto staticNumberOfBits = ::dansandu::farseer::ProtocolSize{33UL};
    };

    int32_t integer;
    bool boolean;
};

}
)";

        const auto expectedSource = R"(#include "organization/artifact/protocol.g.hpp"
#include "dansandu/farseer/binary_serialization.hpp"
#include "dansandu/farseer/protocol_registry.hpp"
#include "dansandu/journey/macro.hpp"

namespace organization::artifact::protocol
{

::dansandu::farseer::ProtocolIdentifier MyMessage::Metadata::getProtocolIdentifier()
{
    return ::dansandu::farseer::ProtocolIdentifier{1986501203U};
}

MyMessage MyMessage::Metadata::deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    auto protocol = MyMessage{};
    protocol.integer = ::dansandu::farseer::binary_serialization::BinarySerializer<int32_t>::deserialize(bytes, bitsOffset);
    protocol.boolean = ::dansandu::farseer::binary_serialization::BinarySerializer<bool>::deserialize(bytes, bitsOffset);
    return protocol;
}

void MyMessage::Metadata::serialize(const MyMessage& protocol, std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    ::dansandu::farseer::binary_serialization::BinarySerializer<int32_t>::serialize(protocol.integer, bytes, bitsOffset);
    ::dansandu::farseer::binary_serialization::BinarySerializer<bool>::serialize(protocol.boolean, bytes, bitsOffset);
}

}

namespace
{

const auto DANSANDU_JOURNEY_UNIQUE_NAME(dansandu_farseer_internal_cpp_protocol_registrar) = 
    ::dansandu::farseer::protocol_registry::ProtocolRegistry::getGlobalInstance().registerMessageProtocol<organization::artifact::protocol::MyMessage>();

}
)";

        const auto protocol = parseProtocolDefinition(text);

        const auto header = generateProtocolCppHeader(protocol);

        REQUIRE(header == expectedHeader);

        const auto source = generateProtocolCppSource(protocol);

        REQUIRE(source == expectedSource);
    }

    SECTION("request protocol")
    {
        const auto text = R"(namespace organization.artifact.protocol;

request MyRequest
{
    string user;
    string password;

    response
    {
        list<string> contacts;
        uint64 authenticationToken;
    }
}
)";

        const auto expectedHeader = R"(#pragma once

#include "dansandu/farseer/common.hpp"

namespace organization::artifact::protocol
{

struct PRALINE_EXPORT MyRequest
{
    struct PRALINE_EXPORT Metadata
    {
        static ::dansandu::farseer::ProtocolIdentifier getProtocolIdentifier();

        static MyRequest deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static void serialize(const MyRequest& protocol, std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static constexpr auto hasStaticSize = false;

        static constexpr auto staticNumberOfBits = ::dansandu::farseer::ProtocolSize{0UL};
    };

    std::string user;
    std::string password;

    struct PRALINE_EXPORT Response
    {
        struct PRALINE_EXPORT Metadata
        {
            static ::dansandu::farseer::ProtocolIdentifier getProtocolIdentifier();

            static Response deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset);

            static void serialize(const Response& protocol, std::vector<uint8_t>& bytes, size_t& bitsOffset);

            static constexpr auto hasStaticSize = false;

            static constexpr auto staticNumberOfBits = ::dansandu::farseer::ProtocolSize{64UL};
        };

        std::vector<std::string> contacts;
        uint64_t authenticationToken;
    };
};

}
)";

        const auto expectedSource = R"(#include "organization/artifact/protocol.g.hpp"
#include "dansandu/farseer/binary_serialization.hpp"
#include "dansandu/farseer/protocol_registry.hpp"
#include "dansandu/journey/macro.hpp"

namespace organization::artifact::protocol
{

::dansandu::farseer::ProtocolIdentifier MyRequest::Metadata::getProtocolIdentifier()
{
    return ::dansandu::farseer::ProtocolIdentifier{1356265941U};
}

MyRequest MyRequest::Metadata::deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    auto protocol = MyRequest{};
    protocol.user = ::dansandu::farseer::binary_serialization::BinarySerializer<std::string>::deserialize(bytes, bitsOffset);
    protocol.password = ::dansandu::farseer::binary_serialization::BinarySerializer<std::string>::deserialize(bytes, bitsOffset);
    return protocol;
}

void MyRequest::Metadata::serialize(const MyRequest& protocol, std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    ::dansandu::farseer::binary_serialization::BinarySerializer<std::string>::serialize(protocol.user, bytes, bitsOffset);
    ::dansandu::farseer::binary_serialization::BinarySerializer<std::string>::serialize(protocol.password, bytes, bitsOffset);
}

::dansandu::farseer::ProtocolIdentifier MyRequest::Response::Metadata::getProtocolIdentifier()
{
    return ::dansandu::farseer::ProtocolIdentifier{1631866348U};
}

MyRequest::Response MyRequest::Response::Metadata::deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    auto protocol = MyRequest::Response{};
    protocol.contacts = ::dansandu::farseer::binary_serialization::BinarySerializer<std::vector<std::string>>::deserialize(bytes, bitsOffset);
    protocol.authenticationToken = ::dansandu::farseer::binary_serialization::BinarySerializer<uint64_t>::deserialize(bytes, bitsOffset);
    return protocol;
}

void MyRequest::Response::Metadata::serialize(const MyRequest::Response& protocol, std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    ::dansandu::farseer::binary_serialization::BinarySerializer<std::vector<std::string>>::serialize(protocol.contacts, bytes, bitsOffset);
    ::dansandu::farseer::binary_serialization::BinarySerializer<uint64_t>::serialize(protocol.authenticationToken, bytes, bitsOffset);
}

}

namespace
{

const auto DANSANDU_JOURNEY_UNIQUE_NAME(dansandu_farseer_internal_cpp_protocol_registrar) = 
    ::dansandu::farseer::protocol_registry::ProtocolRegistry::getGlobalInstance().registerRequestProtocol<organization::artifact::protocol::MyRequest>();

}
)";
        const auto protocol = parseProtocolDefinition(text);

        const auto header = generateProtocolCppHeader(protocol);

        REQUIRE(header == expectedHeader);

        const auto source = generateProtocolCppSource(protocol);

        REQUIRE(source == expectedSource);
    }
}
