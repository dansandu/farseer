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

namespace
{

void insertBack(std::vector<uint8_t>& container, const std::span<const uint8_t> span)
{
    container.insert(container.end(), span.cbegin(), span.cend());
}

}

TEST_CASE("protocol_reader")
{
    const auto receivingSocketIdentifier = SocketIdentifier{};

    auto pendingBytes = std::vector<uint8_t>{};

    auto outboundBuffer = std::vector<uint8_t>{};

    auto protocolReader = ProtocolReader{[&](const SocketIdentifier, std::vector<uint8_t>&& bytes)
                                         { insertBack(outboundBuffer, bytes); }};

    SECTION("empty message")
    {
        const auto bytes = EmptyMessage::Metadata::serializeWithHeader(EmptyMessage{});

        auto protocol = std::any{};

        protocolReader.registerMessageConsumer(EmptyMessage::Metadata::getProtocolIdentifier(),
                                               [&protocol](std::any&& receivedMessage)
                                               { protocol = std::move(receivedMessage); });

        protocolReader.read(receivingSocketIdentifier, pendingBytes, bytes);

        REQUIRE(protocol.has_value());

        const auto& actualMessage = std::any_cast<const EmptyMessage&>(protocol);

        static_cast<void>(actualMessage);

        REQUIRE(pendingBytes.empty());

        REQUIRE(outboundBuffer.empty());
    }

    SECTION("static message")
    {
        const auto message = StaticMessage{
            .integer = -129,
            .boolean = true,
        };

        const auto bytes = StaticMessage::Metadata::serializeWithHeader(message);

        auto protocol = std::any{};

        protocolReader.registerMessageConsumer(StaticMessage::Metadata::getProtocolIdentifier(),
                                               [&protocol](std::any&& receivedMessage)
                                               { protocol = std::move(receivedMessage); });

        protocolReader.read(receivingSocketIdentifier, pendingBytes, bytes);

        REQUIRE(protocol.has_value());

        const auto& actualMessage = std::any_cast<const StaticMessage&>(protocol);

        REQUIRE(actualMessage.integer == message.integer);

        REQUIRE(actualMessage.boolean == message.boolean);

        REQUIRE(pendingBytes.empty());

        REQUIRE(outboundBuffer.empty());
    }

    SECTION("partial static message")
    {
        const auto message = StaticMessage{
            .integer = 493042,
            .boolean = false,
        };

        const auto bytes = StaticMessage::Metadata::serializeWithHeader(message);

        auto protocol = std::any{};

        protocolReader.registerMessageConsumer(StaticMessage::Metadata::getProtocolIdentifier(),
                                               [&protocol](std::any&& receivedMessage)
                                               { protocol = std::move(receivedMessage); });

        const auto halfBytesCount = bytes.size() / 2;

        const auto bytesFirstHalf = std::span<const uint8_t>(bytes.cbegin(), bytes.cbegin() + halfBytesCount);

        const auto bytesSecondHalf = std::span<const uint8_t>(bytes.cbegin() + halfBytesCount, bytes.cend());

        protocolReader.read(receivingSocketIdentifier, pendingBytes, bytesFirstHalf);

        REQUIRE(!protocol.has_value());

        REQUIRE(pendingBytes.size() == bytesFirstHalf.size());

        REQUIRE(outboundBuffer.empty());

        protocolReader.read(receivingSocketIdentifier, pendingBytes, bytesSecondHalf);

        REQUIRE(protocol.has_value());

        const auto& actualMessage = std::any_cast<const StaticMessage&>(protocol);

        REQUIRE(actualMessage.integer == message.integer);

        REQUIRE(actualMessage.boolean == message.boolean);

        REQUIRE(pendingBytes.empty());

        REQUIRE(outboundBuffer.empty());
    }

    SECTION("dynamic message")
    {
        const auto message = DynamicMessage{
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

        const auto bytes = DynamicMessage::Metadata::serializeWithHeader(message);

        auto protocol = std::any{};

        protocolReader.registerMessageConsumer(DynamicMessage::Metadata::getProtocolIdentifier(),
                                               [&protocol](std::any&& receivedMessage)
                                               { protocol = std::move(receivedMessage); });

        protocolReader.read(receivingSocketIdentifier, pendingBytes, bytes);

        REQUIRE(protocol.has_value());

        const auto& actualMessage = std::any_cast<const DynamicMessage&>(protocol);

        REQUIRE(actualMessage.messages.size() == message.messages.size());

        REQUIRE(actualMessage.messages.at(0).integer == message.messages.at(0).integer);

        REQUIRE(actualMessage.messages.at(0).boolean == message.messages.at(0).boolean);

        REQUIRE(actualMessage.messages.at(1).integer == message.messages.at(1).integer);

        REQUIRE(actualMessage.messages.at(1).boolean == message.messages.at(1).boolean);

        REQUIRE(actualMessage.name == message.name);

        REQUIRE(pendingBytes.empty());

        REQUIRE(outboundBuffer.empty());
    }

    SECTION("interleaved partial messages from sockets sharing a reader")
    {
        const auto firstSocketIdentifier = SocketIdentifier{1};

        const auto secondSocketIdentifier = SocketIdentifier{2};

        auto firstPendingBytes = std::vector<uint8_t>{};

        auto secondPendingBytes = std::vector<uint8_t>{};

        const auto firstMessage = StaticMessage{
            .integer = 1111,
            .boolean = true,
        };

        const auto secondMessage = StaticMessage{
            .integer = -2222,
            .boolean = false,
        };

        const auto firstBytes = StaticMessage::Metadata::serializeWithHeader(firstMessage);

        const auto secondBytes = StaticMessage::Metadata::serializeWithHeader(secondMessage);

        auto messages = std::vector<StaticMessage>{};

        protocolReader.registerMessageConsumer(
            StaticMessage::Metadata::getProtocolIdentifier(), [&messages](std::any&& receivedMessage)
            { messages.push_back(std::any_cast<const StaticMessage&>(receivedMessage)); });

        const auto firstHalfBytesCount = firstBytes.size() / 2uz;

        const auto secondHalfBytesCount = secondBytes.size() / 2uz;

        protocolReader.read(firstSocketIdentifier, firstPendingBytes,
                            std::span<const uint8_t>(firstBytes.cbegin(), firstBytes.cbegin() + firstHalfBytesCount));

        protocolReader.read(
            secondSocketIdentifier, secondPendingBytes,
            std::span<const uint8_t>(secondBytes.cbegin(), secondBytes.cbegin() + secondHalfBytesCount));

        REQUIRE(firstPendingBytes.size() == firstHalfBytesCount);

        REQUIRE(secondPendingBytes.size() == secondHalfBytesCount);

        protocolReader.read(firstSocketIdentifier, firstPendingBytes,
                            std::span<const uint8_t>(firstBytes.cbegin() + firstHalfBytesCount, firstBytes.cend()));

        protocolReader.read(secondSocketIdentifier, secondPendingBytes,
                            std::span<const uint8_t>(secondBytes.cbegin() + secondHalfBytesCount, secondBytes.cend()));

        REQUIRE(messages.size() == 2uz);

        REQUIRE(messages.at(0).integer == firstMessage.integer);

        REQUIRE(messages.at(0).boolean == firstMessage.boolean);

        REQUIRE(messages.at(1).integer == secondMessage.integer);

        REQUIRE(messages.at(1).boolean == secondMessage.boolean);

        REQUIRE(firstPendingBytes.empty());

        REQUIRE(secondPendingBytes.empty());

        REQUIRE(outboundBuffer.empty());
    }
}
