#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/sample_protocol.g.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::ProtocolIdentifier;
using dansandu::farseer::binary_serialization::BinarySerializer;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::sample_protocol::DynamicMessage;
using dansandu::farseer::sample_protocol::EmptyMessage;
using dansandu::farseer::sample_protocol::StaticMessage;

TEST_CASE("protocol_reader")
{
    auto bytes = std::vector<uint8_t>{};

    auto bitsCount = size_t{0};

    auto protocolReader = ProtocolReader{};

    SECTION("static message")
    {
        const auto expectedMessage = StaticMessage{
            .integer = -129,
            .boolean = true,
        };

        BinarySerializer<ProtocolIdentifier>::serialize(StaticMessage::Metadata::getProtocolIdentifier(), bytes,
                                                        bitsCount);

        BinarySerializer<StaticMessage>::serialize(expectedMessage, bytes, bitsCount);

        auto receivedProtocol = std::any{};

        protocolReader.registerProtocolConsumer(StaticMessage::Metadata::getProtocolIdentifier(),
                                                [&receivedProtocol](std::any protocol)
                                                { receivedProtocol = protocol; });

        protocolReader.read(bytes);

        REQUIRE(receivedProtocol.has_value());

        const auto actualMessage = std::any_cast<StaticMessage>(receivedProtocol);

        REQUIRE(actualMessage.integer == expectedMessage.integer);

        REQUIRE(actualMessage.boolean == expectedMessage.boolean);
    }

    SECTION("partial static message")
    {
        const auto expectedMessage = StaticMessage{
            .integer = 493042,
            .boolean = false,
        };

        BinarySerializer<ProtocolIdentifier>::serialize(StaticMessage::Metadata::getProtocolIdentifier(), bytes,
                                                        bitsCount);

        BinarySerializer<StaticMessage>::serialize(expectedMessage, bytes, bitsCount);

        auto receivedProtocol = std::any{};

        protocolReader.registerProtocolConsumer(StaticMessage::Metadata::getProtocolIdentifier(),
                                                [&receivedProtocol](std::any protocol)
                                                { receivedProtocol = protocol; });

        const auto halfBytesCount = bytes.size() / 2;

        const auto firstHalfBytes = std::vector<uint8_t>(bytes.cbegin(), bytes.cbegin() + halfBytesCount);

        const auto secondHalfBytes = std::vector<uint8_t>(bytes.cbegin() + halfBytesCount, bytes.cend());

        protocolReader.read(firstHalfBytes);

        REQUIRE(!receivedProtocol.has_value());

        protocolReader.read(secondHalfBytes);

        REQUIRE(receivedProtocol.has_value());

        const auto actualMessage = std::any_cast<StaticMessage>(receivedProtocol);

        REQUIRE(actualMessage.integer == expectedMessage.integer);

        REQUIRE(actualMessage.boolean == expectedMessage.boolean);
    }
}
