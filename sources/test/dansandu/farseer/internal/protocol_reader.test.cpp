#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/sample_protocol.g.hpp"
#include "dansandu/journey/common.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::Expected;
using dansandu::farseer::ProtocolIdentifier;
using dansandu::farseer::ProtocolSequenceNumber;
using dansandu::farseer::SocketIdentifier;
using dansandu::farseer::exception::ProtocolConsumerAlreadyRegisteredError;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::sample_protocol::DynamicMessage;
using dansandu::farseer::sample_protocol::EmptyMessage;
using dansandu::farseer::sample_protocol::MyRequest;
using dansandu::farseer::sample_protocol::StaticMessage;
using dansandu::journey::Level;
using dansandu::radiance::Log;

TEST_CASE("protocol_reader")
{
    auto inboundBytes = std::vector<uint8_t>{};

    auto outboundBytes = std::vector<uint8_t>{};

    auto protocolReader = ProtocolReader{};

    SECTION("empty message")
    {
        EmptyMessage::Metadata::serializeWithHeader(EmptyMessage{}, inboundBytes);

        auto protocol = std::any{};

        protocolReader.registerInboundMessageConsumer(
            EmptyMessage::Metadata::getProtocolIdentifier(),
            [&protocol](std::any&& inboundMessage) { protocol = std::move(inboundMessage); }
        );

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(protocol.has_value());

        const auto& actualMessage = std::any_cast<const EmptyMessage&>(protocol);

        static_cast<void>(actualMessage);

        REQUIRE(inboundBytes.empty());

        REQUIRE(outboundBytes.empty());
    }

    SECTION("static message")
    {
        const auto message = StaticMessage{
            .integer = -129,
            .boolean = true,
        };

        StaticMessage::Metadata::serializeWithHeader(message, inboundBytes);

        auto protocol = std::any{};

        protocolReader.registerInboundMessageConsumer(
            StaticMessage::Metadata::getProtocolIdentifier(),
            [&protocol](std::any&& inboundMessage) { protocol = std::move(inboundMessage); }
        );

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(protocol.has_value());

        const auto& actualMessage = std::any_cast<const StaticMessage&>(protocol);

        REQUIRE(actualMessage.integer == message.integer);

        REQUIRE(actualMessage.boolean == message.boolean);

        REQUIRE(inboundBytes.empty());

        REQUIRE(outboundBytes.empty());
    }

    SECTION("partial static message")
    {
        const auto message = StaticMessage{
            .integer = 493042,
            .boolean = false,
        };

        const auto messageBytes = StaticMessage::Metadata::serializeWithHeader(message);

        auto protocol = std::any{};

        protocolReader.registerInboundMessageConsumer(
            StaticMessage::Metadata::getProtocolIdentifier(),
            [&protocol](std::any&& inboundMessage) { protocol = std::move(inboundMessage); }
        );

        const auto halfBytesCount = messageBytes.size() / 2;

        inboundBytes.insert(inboundBytes.end(), messageBytes.cbegin(), messageBytes.cbegin() + halfBytesCount);

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(!protocol.has_value());

        REQUIRE(inboundBytes.size() == halfBytesCount);

        REQUIRE(outboundBytes.empty());

        inboundBytes.insert(inboundBytes.end(), messageBytes.cbegin() + halfBytesCount, messageBytes.cend());

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(protocol.has_value());

        const auto& actualMessage = std::any_cast<const StaticMessage&>(protocol);

        REQUIRE(actualMessage.integer == message.integer);

        REQUIRE(actualMessage.boolean == message.boolean);

        REQUIRE(inboundBytes.empty());

        REQUIRE(outboundBytes.empty());
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

        DynamicMessage::Metadata::serializeWithHeader(message, inboundBytes);

        auto protocol = std::any{};

        protocolReader.registerInboundMessageConsumer(
            DynamicMessage::Metadata::getProtocolIdentifier(),
            [&protocol](std::any&& inboundMessage) { protocol = std::move(inboundMessage); }
        );

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(protocol.has_value());

        const auto& actualMessage = std::any_cast<const DynamicMessage&>(protocol);

        REQUIRE(actualMessage.messages.size() == message.messages.size());

        REQUIRE(actualMessage.messages.at(0).integer == message.messages.at(0).integer);

        REQUIRE(actualMessage.messages.at(0).boolean == message.messages.at(0).boolean);

        REQUIRE(actualMessage.messages.at(1).integer == message.messages.at(1).integer);

        REQUIRE(actualMessage.messages.at(1).boolean == message.messages.at(1).boolean);

        REQUIRE(actualMessage.name == message.name);

        REQUIRE(inboundBytes.empty());

        REQUIRE(outboundBytes.empty());
    }

    SECTION("successful request")
    {
        const auto request = MyRequest{
            .user = "Sophie",
            .password = "thecatis2feetlong",
        };

        const auto response = Expected<MyRequest::Response>::fromSuccess(
            MyRequest::Response{
                .contacts = {"Sam", "Jim", "Marie"},
                .authenticationToken = 0xA5F14C71,
            }
        );

        const auto sequenceNumber = ProtocolSequenceNumber{17};

        MyRequest::Metadata::serializeWithHeader(request, sequenceNumber, inboundBytes);

        const auto responseBytes = MyRequest::Response::Metadata::serializeWithHeader(response, sequenceNumber);

        auto protocol = std::any{};

        protocolReader.registerInboundRequestConsumer(
            MyRequest::Metadata::getProtocolIdentifier(),
            [&protocol, &response](std::any&& inboundRequest)
            {
                protocol = std::move(inboundRequest);
                return std::make_any<Expected<MyRequest::Response>>(response);
            }
        );

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(protocol.has_value());

        const auto& actualRequest = std::any_cast<const MyRequest&>(protocol);

        REQUIRE(actualRequest.user == request.user);

        REQUIRE(actualRequest.password == request.password);

        REQUIRE(inboundBytes.empty());

        REQUIRE(outboundBytes == responseBytes);
    }

    SECTION("failed request")
    {
        const auto request = MyRequest{
            .user = "Jason",
            .password = "123456",
        };

        const auto response = Expected<MyRequest::Response>::fromFailure(13, "Invalid password");

        const auto sequenceNumber = ProtocolSequenceNumber{49388};

        MyRequest::Metadata::serializeWithHeader(request, sequenceNumber, inboundBytes);

        const auto responseBytes = MyRequest::Response::Metadata::serializeWithHeader(response, sequenceNumber);

        auto protocol = std::any{};

        protocolReader.registerInboundRequestConsumer(
            MyRequest::Metadata::getProtocolIdentifier(),
            [&protocol, &response](std::any&& inboundRequest)
            {
                protocol = std::move(inboundRequest);
                return std::make_any<Expected<MyRequest::Response>>(response);
            }
        );

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(protocol.has_value());

        const auto& actualRequest = std::any_cast<const MyRequest&>(protocol);

        REQUIRE(actualRequest.user == request.user);

        REQUIRE(actualRequest.password == request.password);

        REQUIRE(inboundBytes.empty());

        REQUIRE(outboundBytes == responseBytes);
    }

    SECTION("successful response")
    {
        const auto response = Expected<MyRequest::Response>::fromSuccess(
            MyRequest::Response{
                .contacts = {"Sam", "Jim", "Marie"},
                .authenticationToken = 0xA5F14C71,
            }
        );

        const auto sequenceNumber = ProtocolSequenceNumber{1111};

        MyRequest::Response::Metadata::serializeWithHeader(response, sequenceNumber, inboundBytes);

        auto protocol = std::any{};

        protocolReader.registerInboundOneShotResponseConsumer(
            sequenceNumber, [&protocol](std::any&& inboundResponse) { protocol = std::move(inboundResponse); }
        );

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(protocol.has_value());

        const auto& actualResponse = std::any_cast<const Expected<MyRequest::Response>&>(protocol);

        REQUIRE(actualResponse.success());

        REQUIRE(actualResponse.getValue().contacts == response.getValue().contacts);

        REQUIRE(actualResponse.getValue().authenticationToken == response.getValue().authenticationToken);

        REQUIRE(inboundBytes.empty());

        REQUIRE(outboundBytes.empty());
    }

    SECTION("failed response")
    {
        const auto response = Expected<MyRequest::Response>::fromFailure(13, "Invalid password");

        const auto sequenceNumber = ProtocolSequenceNumber{68958};

        MyRequest::Response::Metadata::serializeWithHeader(response, sequenceNumber, inboundBytes);

        auto protocol = std::any{};

        protocolReader.registerInboundOneShotResponseConsumer(
            sequenceNumber, [&protocol](std::any&& inboundResponse) { protocol = std::move(inboundResponse); }
        );

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(protocol.has_value());

        const auto& actualResponse = std::any_cast<const Expected<MyRequest::Response>&>(protocol);

        REQUIRE(actualResponse.failure());

        REQUIRE(actualResponse.getErrorCode() == response.getErrorCode());

        REQUIRE(actualResponse.getErrorMessage() == response.getErrorMessage());

        REQUIRE(inboundBytes.empty());

        REQUIRE(outboundBytes.empty());
    }

    SECTION("partial request")
    {
        const auto request = MyRequest{
            .user = "Emma",
            .password = "correcthorsebatterystaple",
        };

        const auto response = Expected<MyRequest::Response>::fromSuccess(
            MyRequest::Response{
                .contacts = {"Liam"},
                .authenticationToken = 0x1F2E3D4C,
            }
        );

        const auto sequenceNumber = ProtocolSequenceNumber{902};

        const auto requestBytes = MyRequest::Metadata::serializeWithHeader(request, sequenceNumber);

        const auto responseBytes = MyRequest::Response::Metadata::serializeWithHeader(response, sequenceNumber);

        auto protocol = std::any{};

        protocolReader.registerInboundRequestConsumer(
            MyRequest::Metadata::getProtocolIdentifier(),
            [&protocol, &response](std::any&& inboundRequest)
            {
                protocol = std::move(inboundRequest);
                return std::make_any<Expected<MyRequest::Response>>(response);
            }
        );

        const auto halfBytesCount = requestBytes.size() / 2uz;

        inboundBytes.insert(inboundBytes.end(), requestBytes.cbegin(), requestBytes.cbegin() + halfBytesCount);

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(!protocol.has_value());

        REQUIRE(inboundBytes.size() == halfBytesCount);

        REQUIRE(outboundBytes.empty());

        inboundBytes.insert(inboundBytes.end(), requestBytes.cbegin() + halfBytesCount, requestBytes.cend());

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(protocol.has_value());

        const auto& actualRequest = std::any_cast<const MyRequest&>(protocol);

        REQUIRE(actualRequest.user == request.user);

        REQUIRE(actualRequest.password == request.password);

        REQUIRE(inboundBytes.empty());

        REQUIRE(outboundBytes == responseBytes);
    }

    SECTION("one shot response consumer is removed after use")
    {
        const auto response = Expected<MyRequest::Response>::fromSuccess(
            MyRequest::Response{
                .contacts = {"Ava"},
                .authenticationToken = 42,
            }
        );

        const auto sequenceNumber = ProtocolSequenceNumber{3030};

        const auto responseBytes = MyRequest::Response::Metadata::serializeWithHeader(response, sequenceNumber);

        const auto noConsumerLogs = std::vector<Log>{
            Log{Level::debug, L"Successfully read the response protocol 1498178261"},
            Log{Level::error, L"The response protocol with identifier 1498178261 and sequence number 3030 has no "
                              L"consumer registered and will be skipped"},
        };

        inboundBytes = responseBytes;

        REQUIRE_LOG(noConsumerLogs, protocolReader.readInboundBytes(inboundBytes, outboundBytes));

        auto consumerCalls = 0;

        const auto consumer = [&consumerCalls](std::any&&) { ++consumerCalls; };

        protocolReader.registerInboundOneShotResponseConsumer(sequenceNumber, consumer);

        REQUIRE_THROW(
            ProtocolConsumerAlreadyRegisteredError,
            protocolReader.registerInboundOneShotResponseConsumer(sequenceNumber, consumer)
        );

        inboundBytes = responseBytes;

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(consumerCalls == 1);

        REQUIRE(inboundBytes.empty());

        REQUIRE(outboundBytes.empty());

        inboundBytes = responseBytes;

        REQUIRE_LOG(noConsumerLogs, protocolReader.readInboundBytes(inboundBytes, outboundBytes));
    }

    SECTION("interleaved partial messages from sockets sharing a reader")
    {
        auto firstInboundBytes = std::vector<uint8_t>{};

        auto secondInboundBytes = std::vector<uint8_t>{};

        auto firstOutboundBytes = std::vector<uint8_t>{};

        auto secondOutboundBytes = std::vector<uint8_t>{};

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

        protocolReader.registerInboundMessageConsumer(
            StaticMessage::Metadata::getProtocolIdentifier(), [&messages](std::any&& inboundMessage)
            { messages.push_back(std::any_cast<const StaticMessage&>(inboundMessage)); }
        );

        const auto firstHalfBytesCount = firstBytes.size() / 2uz;

        const auto secondHalfBytesCount = secondBytes.size() / 2uz;

        firstInboundBytes.insert(
            firstInboundBytes.end(), firstBytes.cbegin(), firstBytes.cbegin() + firstHalfBytesCount
        );

        secondInboundBytes.insert(
            secondInboundBytes.end(), secondBytes.cbegin(), secondBytes.cbegin() + secondHalfBytesCount
        );

        protocolReader.readInboundBytes(firstInboundBytes, firstOutboundBytes);

        protocolReader.readInboundBytes(secondInboundBytes, secondOutboundBytes);

        REQUIRE(firstInboundBytes.size() == firstHalfBytesCount);

        REQUIRE(secondInboundBytes.size() == secondHalfBytesCount);

        firstInboundBytes.insert(firstInboundBytes.end(), firstBytes.cbegin() + firstHalfBytesCount, firstBytes.cend());

        secondInboundBytes.insert(
            secondInboundBytes.end(), secondBytes.cbegin() + secondHalfBytesCount, secondBytes.cend()
        );

        protocolReader.readInboundBytes(firstInboundBytes, firstOutboundBytes);

        protocolReader.readInboundBytes(secondInboundBytes, secondOutboundBytes);

        REQUIRE(messages.size() == 2uz);

        REQUIRE(messages.at(0).integer == firstMessage.integer);

        REQUIRE(messages.at(0).boolean == firstMessage.boolean);

        REQUIRE(messages.at(1).integer == secondMessage.integer);

        REQUIRE(messages.at(1).boolean == secondMessage.boolean);

        REQUIRE(firstInboundBytes.empty());

        REQUIRE(secondInboundBytes.empty());

        REQUIRE(firstOutboundBytes.empty());

        REQUIRE(secondOutboundBytes.empty());
    }

    SECTION("reads all available protocols in inbound bytes")
    {
        const auto firstMessage = StaticMessage{
            .integer = -3894,
            .boolean = false,
        };

        const auto secondMessage = StaticMessage{
            .integer = 7198,
            .boolean = true,
        };

        const auto thirdMessage = StaticMessage{
            .integer = -99952,
            .boolean = true,
        };

        const auto firstBytes = StaticMessage::Metadata::serializeWithHeader(firstMessage);

        const auto secondBytes = StaticMessage::Metadata::serializeWithHeader(secondMessage);

        const auto thirdBytes = StaticMessage::Metadata::serializeWithHeader(thirdMessage);

        const auto thirdHalfBytesCount = thirdBytes.size() / 2uz;

        inboundBytes = firstBytes;

        inboundBytes.insert(inboundBytes.end(), secondBytes.cbegin(), secondBytes.cend());

        inboundBytes.insert(inboundBytes.end(), thirdBytes.cbegin(), thirdBytes.cbegin() + thirdHalfBytesCount);

        auto messages = std::vector<StaticMessage>{};

        protocolReader.registerInboundMessageConsumer(
            StaticMessage::Metadata::getProtocolIdentifier(), [&messages](std::any&& inboundMessage)
            { messages.push_back(std::any_cast<const StaticMessage&>(inboundMessage)); }
        );

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(messages.size() == 2uz);

        REQUIRE(messages.at(0).integer == firstMessage.integer);

        REQUIRE(messages.at(0).boolean == firstMessage.boolean);

        REQUIRE(messages.at(1).integer == secondMessage.integer);

        REQUIRE(messages.at(1).boolean == secondMessage.boolean);

        REQUIRE(inboundBytes.size() == thirdHalfBytesCount);

        REQUIRE(outboundBytes.empty());

        inboundBytes.insert(inboundBytes.end(), thirdBytes.cbegin() + thirdHalfBytesCount, thirdBytes.cend());

        protocolReader.readInboundBytes(inboundBytes, outboundBytes);

        REQUIRE(messages.size() == 3uz);

        REQUIRE(messages.at(2).integer == thirdMessage.integer);

        REQUIRE(messages.at(2).boolean == thirdMessage.boolean);

        REQUIRE(inboundBytes.empty());

        REQUIRE(outboundBytes.empty());
    }

    SECTION("second message is consumed even if the first message consumer throws")
    {
        const auto expectedMessages = std::vector<StaticMessage>{
            StaticMessage{
                .integer = 20261002,
                .boolean = false,
            },
            StaticMessage{
                .integer = 2139,
                .boolean = false,
            }
        };

        auto actualMessages = std::vector<StaticMessage>{};

        protocolReader.registerInboundMessageConsumer(
            StaticMessage::Metadata::getProtocolIdentifier(),
            [&, calls = 0](std::any&& inboundMessage) mutable
            {
                const auto& actualMessage = std::any_cast<const StaticMessage&>(inboundMessage);

                actualMessages.push_back(actualMessage);

                ++calls;

                if (calls == 1)
                {
                    throw std::runtime_error{"oopsie"};
                }
            }
        );

        const auto firstBytes = StaticMessage::Metadata::serializeWithHeader(expectedMessages.at(0));

        const auto secondBytes = StaticMessage::Metadata::serializeWithHeader(expectedMessages.at(1));

        inboundBytes = firstBytes;

        inboundBytes.insert(inboundBytes.end(), secondBytes.cbegin(), secondBytes.cend());

        const auto expectedLogs = std::vector<Log>{
            Log{Level::debug, L"Successfully read the message protocol 3028821238"},
            Log{Level::error, L"While invoking the consumer for the protocol with identifier 3028821238 an exception "
                              L"was caught with the message: oopsie"},
            Log{Level::debug, L"Successfully read the message protocol 3028821238"},
        };

        REQUIRE_LOG(expectedLogs, protocolReader.readInboundBytes(inboundBytes, outboundBytes));

        REQUIRE(expectedMessages.at(0).integer == actualMessages.at(0).integer);

        REQUIRE(expectedMessages.at(0).boolean == actualMessages.at(0).boolean);

        REQUIRE(expectedMessages.at(1).integer == actualMessages.at(1).integer);

        REQUIRE(expectedMessages.at(1).boolean == actualMessages.at(1).boolean);

        REQUIRE(inboundBytes.empty());

        REQUIRE(outboundBytes.empty());
    }
}
