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
    i32 integer;
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

        static void serializeHeaderless(const MyMessage& message, std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static MyMessage deserializeHeaderless(const std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static std::vector<uint8_t> serializeWithHeader(const MyMessage& message);

        static bool tryDeserializeWithHeader(const std::vector<uint8_t>& bytes, size_t& bitsOffset, std::any& message);

        static constexpr auto hasStaticSize = true;

        static constexpr auto staticNumberOfBits = ::dansandu::farseer::ProtocolSize{33UL};
    };

    int32_t integer;
    bool boolean;
};

}
)";

        const auto expectedSource = R"(#include "organization/artifact/protocol.g.hpp"
#include "dansandu/ballotin/binary.hpp"
#include "dansandu/farseer/binary_serialization.hpp"
#include "dansandu/farseer/protocol_registry.hpp"
#include "dansandu/journey/macro.hpp"

namespace organization::artifact::protocol
{

::dansandu::farseer::ProtocolIdentifier MyMessage::Metadata::getProtocolIdentifier()
{
    return ::dansandu::farseer::ProtocolIdentifier{1986501203U};
}

void MyMessage::Metadata::serializeHeaderless(const MyMessage& message, std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    ::dansandu::farseer::binary_serialization::BinarySerializer<int32_t>::serialize(message.integer, bytes, bitsOffset);
    ::dansandu::farseer::binary_serialization::BinarySerializer<bool>::serialize(message.boolean, bytes, bitsOffset);
}

MyMessage MyMessage::Metadata::deserializeHeaderless(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    auto message = MyMessage{};
    message.integer = ::dansandu::farseer::binary_serialization::BinarySerializer<int32_t>::deserialize(bytes, bitsOffset);
    message.boolean = ::dansandu::farseer::binary_serialization::BinarySerializer<bool>::deserialize(bytes, bitsOffset);
    return message;
}

std::vector<uint8_t> MyMessage::Metadata::serializeWithHeader(const MyMessage& message)
{
    using ::dansandu::farseer::binary_serialization::BinarySerializer;
    using ::dansandu::farseer::ProtocolIdentifier;

    auto bytes = std::vector<uint8_t>{};
    auto bitsOffset = size_t{0};

    BinarySerializer<ProtocolIdentifier>::serialize(MyMessage::Metadata::getProtocolIdentifier(), bytes, bitsOffset);

    if constexpr (MyMessage::Metadata::hasStaticSize)
    {
        BinarySerializer<MyMessage>::serialize(message, bytes, bitsOffset);
    }
    else
    {
        using ::dansandu::ballotin::binary::bitsPerByte;
        using ::dansandu::ballotin::binary::pushBitsMostSignificant;
        using ::dansandu::farseer::getProtocolSizeFromStdSize;
        using ::dansandu::farseer::ProtocolSize;

        auto dynamicBytes = std::vector<uint8_t>{};
        auto dynamicBitsOffset = size_t{0};

        BinarySerializer<MyMessage>::serialize(message, dynamicBytes, dynamicBitsOffset);

        BinarySerializer<ProtocolSize>::serialize(getProtocolSizeFromStdSize(dynamicBitsOffset), bytes, bitsOffset);

        for (const auto byte : dynamicBytes)
        {
            pushBitsMostSignificant(bytes, bitsOffset, byte, bitsPerByte);
        }
    }

    return bytes;
}

bool MyMessage::Metadata::tryDeserializeWithHeader(const std::vector<uint8_t>& bytes, size_t& bitsOffset, std::any& message)
{
    using ::dansandu::ballotin::binary::bitsPerByte;
    using ::dansandu::farseer::binary_serialization::BinarySerializer;

    if constexpr (MyMessage::Metadata::hasStaticSize)
    {
        if (bitsPerByte * bytes.size() >= bitsOffset + MyMessage::Metadata::staticNumberOfBits.getUnderlying())
        {
            message = BinarySerializer<MyMessage>::deserialize(bytes, bitsOffset);
            return true;
        }
    }
    else
    {
        if (bitsPerByte * bytes.size() >= bitsOffset + bitsPerByte * sizeof(ProtocolSize))
        {
            const auto dynamicNumberOfBits = BinarySerializer<ProtocolSize>::deserialize(bytes, bitsOffset);

            if (bitsPerByte * bytes.size() >= bitsOffset + dynamicNumberOfBits.getUnderlying())
            {
                message = BinarySerializer<MyMessage>::deserialize(bytes, bitsOffset);
                return true;
            }
        }
    }
    return false;
}

}

namespace
{

const auto DANSANDU_JOURNEY_UNIQUE_NAME =
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
        u64 authenticationToken;
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

        static void serializeHeaderless(const MyRequest& request, std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static MyRequest deserializeHeaderless(const std::vector<uint8_t>& bytes, size_t& bitsOffset);

        static std::vector<uint8_t> serializeWithHeader(const MyRequest& request, const ::dansandu::farseer::ProtocolSequenceNumber sequenceNumber);

        static bool tryDeserializeWithHeader(const std::vector<uint8_t>& bytes, size_t& bitsOffset, ::dansandu::farseer::ProtocolSequenceNumber& sequenceNumber, std::any& request);

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

            static void serializeHeaderless(const Response& response, std::vector<uint8_t>& bytes, size_t& bitsOffset);

            static Response deserializeHeaderless(const std::vector<uint8_t>& bytes, size_t& bitsOffset);

            static std::vector<uint8_t> serializeWithHeader(const std::any& response, const ::dansandu::farseer::ProtocolSequenceNumber sequenceNumber);

            static bool tryDeserializeWithHeader(const std::vector<uint8_t>& bytes, size_t& bitsOffset, ::dansandu::farseer::ProtocolSequenceNumber& sequenceNumber, std::any& response);

            static std::any invokeCallback(const UniqueFunction<Response(MyRequest&&)>& callback, std::any&& request);

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
#include "dansandu/ballotin/binary.hpp"
#include "dansandu/farseer/binary_serialization.hpp"
#include "dansandu/farseer/protocol_registry.hpp"
#include "dansandu/journey/macro.hpp"

namespace organization::artifact::protocol
{

::dansandu::farseer::ProtocolIdentifier MyRequest::Metadata::getProtocolIdentifier()
{
    return ::dansandu::farseer::ProtocolIdentifier{1356265941U};
}

void MyRequest::Metadata::serializeHeaderless(const MyRequest& request, std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    ::dansandu::farseer::binary_serialization::BinarySerializer<std::string>::serialize(request.user, bytes, bitsOffset);
    ::dansandu::farseer::binary_serialization::BinarySerializer<std::string>::serialize(request.password, bytes, bitsOffset);
}

MyRequest MyRequest::Metadata::deserializeHeaderless(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    auto request = MyRequest{};
    request.user = ::dansandu::farseer::binary_serialization::BinarySerializer<std::string>::deserialize(bytes, bitsOffset);
    request.password = ::dansandu::farseer::binary_serialization::BinarySerializer<std::string>::deserialize(bytes, bitsOffset);
    return request;
}

std::vector<uint8_t> MyRequest::Metadata::serializeWithHeader(const MyRequest& request, const ::dansandu::farseer::ProtocolSequenceNumber sequenceNumber)
{
    using ::dansandu::farseer::binary_serialization::BinarySerializer;
    using ::dansandu::farseer::ProtocolSequenceNumber;

    auto bytes = std::vector<uint8_t>{};
    auto bitsOffset = size_t{0};

    BinarySerializer<ProtocolIdentifier>::serialize(MyRequest::Metadata::getProtocolIdentifier(), bytes, bitsOffset);

    BinarySerializer<ProtocolSequenceNumber>::serialize(sequenceNumber, bytes, bitsOffset);

    if constexpr (MyRequest::Metadata::hasStaticSize)
    {
        BinarySerializer<MyRequest>::serialize(request, bytes, bitsOffset);
    }
    else
    {
        using ::dansandu::ballotin::binary::bitsPerByte;
        using ::dansandu::ballotin::binary::pushBitsMostSignificant;
        using ::dansandu::farseer::getProtocolSizeFromStdSize;
        using ::dansandu::farseer::ProtocolSize;

        auto dynamicBytes = std::vector<uint8_t>{};
        auto dynamicBitsOffset = size_t{0};

        BinarySerializer<MyRequest>::serialize(request, dynamicBytes, dynamicBitsOffset);

        BinarySerializer<ProtocolSize>::serialize(getProtocolSizeFromStdSize(dynamicBitsOffset), bytes, bitsOffset);

        for (const auto byte : dynamicBytes)
        {
            pushBitsMostSignificant(bytes, bitsOffset, byte, bitsPerByte);
        }
    }

    return bytes;
}

bool MyRequest::Metadata::tryDeserializeWithHeader(const std::vector<uint8_t>& bytes, size_t& bitsOffset, ::dansandu::farseer::ProtocolSequenceNumber& sequenceNumber, std::any& request)
{
    using ::dansandu::ballotin::binary::bitsPerByte;
    using ::dansandu::farseer::binary_serialization::BinarySerializer;

    if constexpr (MyRequest::Metadata::hasStaticSize)
    {
        if (bitsPerByte * bytes.size() >= bitsOffset + bitsPerByte * sizeof(ProtocolSequenceNumber) +
                                              MyRequest::Metadata::staticNumberOfBits.getUnderlying())
        {
            sequenceNumber = BinarySerializer<ProtocolSequenceNumber>::deserialize(bytes, bitsOffset);
            request = BinarySerializer<MyRequest>::deserialize(bytes, bitsOffset);
            return true;
        }
    }
    else
    {
        if (bitsPerByte * bytes.size() >=
            bitsOffset + bitsPerByte * sizeof(ProtocolSequenceNumber) + bitsPerByte * sizeof(ProtocolSize))
        {
            sequenceNumber = BinarySerializer<ProtocolSequenceNumber>::deserialize(bytes, bitsOffset);

            const auto dynamicNumberOfBits = BinarySerializer<ProtocolSize>::deserialize(bytes, bitsOffset);

            if (bitsPerByte * bytes.size() >= bitsOffset + dynamicNumberOfBits.getUnderlying())
            {
                request = BinarySerializer<MyRequest>::deserialize(bytes, bitsOffset);
                return true;
            }
        }
    }
    return false;
}

::dansandu::farseer::ProtocolIdentifier MyRequest::Response::Metadata::getProtocolIdentifier()
{
    return ::dansandu::farseer::ProtocolIdentifier{1631866348U};
}

void MyRequest::Response::Metadata::serializeHeaderless(const MyRequest::Response& response, std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    ::dansandu::farseer::binary_serialization::BinarySerializer<std::vector<std::string>>::serialize(response.contacts, bytes, bitsOffset);
    ::dansandu::farseer::binary_serialization::BinarySerializer<uint64_t>::serialize(response.authenticationToken, bytes, bitsOffset);
}

MyRequest::Response MyRequest::Response::Metadata::deserializeHeaderless(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    auto response = MyRequest::Response{};
    response.contacts = ::dansandu::farseer::binary_serialization::BinarySerializer<std::vector<std::string>>::deserialize(bytes, bitsOffset);
    response.authenticationToken = ::dansandu::farseer::binary_serialization::BinarySerializer<uint64_t>::deserialize(bytes, bitsOffset);
    return response;
}

std::vector<uint8_t> MyRequest::Response::Metadata::serializeWithHeader(const std::any& response, const ::dansandu::farseer::ProtocolSequenceNumber sequenceNumber)
{
    using ::dansandu::farseer::binary_serialization::BinarySerializer;
    using ::dansandu::ballotin::binary::bitsPerByte;
    using ::dansandu::ballotin::binary::pushBitsMostSignificant;
    using ::dansandu::farseer::getProtocolSizeFromStdSize;
    using ::dansandu::farseer::ProtocolSize;
    using ::dansandu::farseer::ProtocolIdentifier;
    using ::dansandu::farseer::ProtocolSequenceNumber;
    using ::dansandu::farseer::Expected;

    const auto& casted = std::any_cast<const Expected<MyRequest::Response>&>(response);

    auto bytes = std::vector<uint8_t>{};
    auto bitsOffset = size_t{0};

    BinarySerializer<ProtocolIdentifier>::serialize(MyRequest::Response::Metadata::getProtocolIdentifier(), bytes, bitsOffset);

    BinarySerializer<ProtocolSequenceNumber>::serialize(sequenceNumber, bytes, bitsOffset);

    auto dynamicBytes = std::vector<uint8_t>{};
    auto dynamicBitsOffset = size_t{0};

    BinarySerializer<Expected<MyRequest::Response>>::serialize(casted, dynamicBytes, dynamicBitsOffset);

    BinarySerializer<ProtocolSize>::serialize(getProtocolSizeFromStdSize(dynamicBitsOffset), bytes, bitsOffset);

    for (const auto byte : dynamicBytes)
    {
        pushBitsMostSignificant(bytes, bitsOffset, byte, bitsPerByte);
    }

    return bytes;
}

bool MyRequest::Response::Metadata::tryDeserializeWithHeader(const std::vector<uint8_t>& bytes, size_t& bitsOffset, ::dansandu::farseer::ProtocolSequenceNumber& sequenceNumber, std::any& response)
{
    using ::dansandu::ballotin::binary::bitsPerByte;
    using ::dansandu::farseer::binary_serialization::BinarySerializer;
    using ::dansandu::farseer::ProtocolSize;
    using ::dansandu::farseer::ProtocolSequenceNumber;
    using ::dansandu::farseer::Expected;

    if (bitsPerByte * bytes.size() >=
        bitsOffset + bitsPerByte * sizeof(ProtocolSequenceNumber) + bitsPerByte * sizeof(ProtocolSize))
    {
        sequenceNumber = BinarySerializer<ProtocolSequenceNumber>::deserialize(bytes, bitsOffset);

        const auto dynamicNumberOfBits = BinarySerializer<ProtocolSize>::deserialize(bytes, bitsOffset);

        if (bitsPerByte * bytes.size() >= bitsOffset + dynamicNumberOfBits.getUnderlying())
        {
            response = BinarySerializer<Expected<MyRequest::Response>>::deserialize(bytes, bitsOffset);
            return true;
        }
    }

    return false;
}

std::any MyRequest::Response::Metadata::invokeCallback(const UniqueFunction<Response(MyRequest&&)>& callback, std::any&& request)
{
    using ::dansandu::farseer::Expected;

    try
    {
        return Expected<Response>::fromSuccess(callback(std::any_cast<MyRequest&&>(std::move(request))));
    }
    catch (const RequestProtocolError& exception)
    {
        return Expected<Response>::fromFailure(exception.getErrorCode(), exception.getErrorMessage());
    }
    catch (const std::exception& exception)
    {
        return Expected<Response>::fromInternalServerError(exception.what());
    }
    catch (...)
    {
        return Expected<Response>::fromInternalServerError();
    }
}

}

namespace
{

const auto DANSANDU_JOURNEY_UNIQUE_NAME =
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
