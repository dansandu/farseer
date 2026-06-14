#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/sample_protocol.g.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::ProtocolIdentifier;
using dansandu::farseer::ProtocolSequenceNumber;
using dansandu::farseer::SocketIdentifier;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::sample_protocol::DynamicMessage;
using dansandu::farseer::sample_protocol::EmptyMessage;
using dansandu::farseer::sample_protocol::StaticMessage;

TEST_CASE("protocol_reader")
{
    const auto receivingSocketIdentifier = SocketIdentifier{};

    auto outboundBuffer = std::vector<uint8_t>{};

    auto protocolReader = ProtocolReader{[&](const SocketIdentifier, std::vector<uint8_t>&& bytes)
                                         { outboundBuffer.insert(outboundBuffer.end(), bytes.begin(), bytes.end()); }};

    SECTION("empty message")
    {
        const auto bytes = EmptyMessage::Metadata::serializeWithHeader(EmptyMessage{});

        auto protocol = std::any{};

        protocolReader.registerMessageConsumer(EmptyMessage::Metadata::getProtocolIdentifier(),
                                               [&protocol](std::any&& message) { protocol = std::move(message); });

        protocolReader.read(receivingSocketIdentifier, bytes);

        REQUIRE(protocol.has_value());

        const auto& message = std::any_cast<const EmptyMessage&>(protocol);

        static_cast<void>(message);
    }

    SECTION("static message")
    {
        const auto expectedMessage = StaticMessage{
            .integer = -129,
            .boolean = true,
        };

        const auto bytes = StaticMessage::Metadata::serializeWithHeader(expectedMessage);

        auto protocol = std::any{};

        protocolReader.registerMessageConsumer(StaticMessage::Metadata::getProtocolIdentifier(),
                                               [&protocol](std::any&& message) { protocol = std::move(message); });

        protocolReader.read(receivingSocketIdentifier, bytes);

        REQUIRE(protocol.has_value());

        const auto& message = std::any_cast<const StaticMessage&>(protocol);

        REQUIRE(message.integer == expectedMessage.integer);

        REQUIRE(message.boolean == expectedMessage.boolean);
    }

    SECTION("partial static message")
    {
        const auto expectedMessage = StaticMessage{
            .integer = 493042,
            .boolean = false,
        };

        const auto bytes = StaticMessage::Metadata::serializeWithHeader(expectedMessage);

        auto protocol = std::any{};

        protocolReader.registerMessageConsumer(StaticMessage::Metadata::getProtocolIdentifier(),
                                               [&protocol](std::any&& message) { protocol = std::move(message); });

        const auto halfBytesCount = bytes.size() / 2;

        const auto bytesFirstHalf = std::vector<uint8_t>(bytes.cbegin(), bytes.cbegin() + halfBytesCount);

        const auto bytesSecondHalf = std::vector<uint8_t>(bytes.cbegin() + halfBytesCount, bytes.cend());

        protocolReader.read(receivingSocketIdentifier, bytesFirstHalf);

        REQUIRE(!protocol.has_value());

        protocolReader.read(receivingSocketIdentifier, bytesSecondHalf);

        REQUIRE(protocol.has_value());

        const auto& message = std::any_cast<const StaticMessage&>(protocol);

        REQUIRE(message.integer == expectedMessage.integer);

        REQUIRE(message.boolean == expectedMessage.boolean);
    }

    SECTION("dynamic message")
    {
        const auto expectedMessage = DynamicMessage{
            .messages =
                {
                    StaticMessage{
                        .integer = 12345,
                        .boolean = false,
                    },
                    StaticMessage{
                        .integer = 67890,
                        .boolean = true,
                    },
                },
            .name = "dynamic message",
        };

        const auto bytes = DynamicMessage::Metadata::serializeWithHeader(expectedMessage);

        auto protocol = std::any{};

        protocolReader.registerMessageConsumer(DynamicMessage::Metadata::getProtocolIdentifier(),
                                               [&protocol](std::any&& message) { protocol = std::move(message); });

        protocolReader.read(receivingSocketIdentifier, bytes);

        REQUIRE(protocol.has_value());

        const auto& message = std::any_cast<const DynamicMessage&>(protocol);

        REQUIRE(message.messages.size() == expectedMessage.messages.size());

        REQUIRE(message.messages.at(0).integer == expectedMessage.messages.at(0).integer);

        REQUIRE(message.messages.at(0).boolean == expectedMessage.messages.at(0).boolean);

        REQUIRE(message.messages.at(1).integer == expectedMessage.messages.at(1).integer);

        REQUIRE(message.messages.at(1).boolean == expectedMessage.messages.at(1).boolean);

        REQUIRE(message.name == expectedMessage.name);
    }
}
