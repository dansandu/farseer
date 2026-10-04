#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/ballotin/binary.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/binary_serialization.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/protocol_registry.hpp"
#include "dansandu/journey/logging.hpp"

using dansandu::ballotin::binary::numberOfBitsToNumberOfBytes;
using dansandu::farseer::binary_serialization::BinarySerializer;
using dansandu::farseer::exception::ProtocolConsumerAlreadyRegisteredError;
using dansandu::farseer::exception::ProtocolNotRegisteredError;
using dansandu::farseer::protocol_registry::ProtocolDescriptor;
using dansandu::farseer::protocol_registry::ProtocolRegistry;
using dansandu::farseer::protocol_registry::ProtocolType;
using dansandu::journey::exception::WideException;

namespace dansandu::farseer::internal::protocol_reader
{

ProtocolReader::ProtocolReader(
    UniqueFunction<void(const SocketIdentifier, std::vector<uint8_t>&&)>&& outboundResponseConsumer
)
    : outboundResponseConsumer_{std::move(outboundResponseConsumer)}
{
}

void ProtocolReader::registerInboundMessageConsumer(
    const ProtocolIdentifier messageIdentifier, UniqueFunction<void(std::any&&)>&& messageConsumer
)
{
    if (!ProtocolRegistry::getGlobalInstance().isProtocolRegistered(messageIdentifier))
    {
        THROW(ProtocolNotRegisteredError, "No message protocol is registered with identifier ", messageIdentifier);
    }

    if (!inboundMessageConsumers_.contains(messageIdentifier))
    {
        inboundMessageConsumers_.insert({messageIdentifier, std::move(messageConsumer)});
    }
    else
    {
        THROW(
            ProtocolConsumerAlreadyRegisteredError, "Another protocol consumer is already registered with identifier ",
            messageIdentifier
        );
    }
}

void ProtocolReader::registerInboundRequestConsumer(
    const ProtocolIdentifier requestIdentifier, UniqueFunction<std::any(std::any&&)>&& requestConsumer
)
{
    if (!ProtocolRegistry::getGlobalInstance().isProtocolRegistered(requestIdentifier))
    {
        THROW(ProtocolNotRegisteredError, "No request protocol is registered with identifier ", requestIdentifier);
    }

    if (!inboundRequestConsumers_.contains(requestIdentifier))
    {
        inboundRequestConsumers_.insert({requestIdentifier, std::move(requestConsumer)});
    }
    else
    {
        THROW(
            ProtocolConsumerAlreadyRegisteredError, "Another protocol consumer is already registered with identifier ",
            requestIdentifier
        );
    }
}

void ProtocolReader::registerInboundOneShotResponseConsumer(
    const ProtocolSequenceNumber sequenceNumber, UniqueFunction<void(std::any&&)>&& responseConsumer
)
{
    if (!inboundOneShotResponseConsumers_.contains(sequenceNumber))
    {
        inboundOneShotResponseConsumers_.insert({sequenceNumber, std::move(responseConsumer)});
    }
    else
    {
        THROW(
            ProtocolConsumerAlreadyRegisteredError,
            "Another response consumer is already registered with sequence number ", sequenceNumber
        );
    }
}

namespace
{

void eraseBits(std::vector<uint8_t>& bytes, const size_t bitsOffset)
{
    const auto numberOfBytesToErase = numberOfBitsToNumberOfBytes(bitsOffset);

    bytes.erase(bytes.cbegin(), bytes.cbegin() + numberOfBytesToErase);
}

void wrapInTryCatchAndInvoke(
    const ProtocolIdentifier protocolIdentifier, const UniqueFunction<void(std::any&&)>& consumer, std::any&& protocol
)
{
    try
    {
        consumer(std::move(protocol));
    }
    catch (const WideException& exception)
    {
        LOG_ERROR(
            "While invoking the consumer for the protocol with identifier ", protocolIdentifier,
            " a wide exception was caught with the message: ", exception.getMessage()
        );
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR(
            "While invoking the consumer for the protocol with identifier ", protocolIdentifier,
            " an exception was caught with the message: ", exception.what()
        );
    }
    catch (...)
    {
        LOG_ERROR(
            "While invoking the consumer for the protocol with identifier ", protocolIdentifier,
            " an unknown exception was caught"
        );
    }
}

}

bool ProtocolReader::readInboundMessage(
    std::vector<uint8_t>& pendingBytes, const ProtocolIdentifier messageIdentifier,
    const ProtocolDescriptor& messageDescriptor, size_t& bitsOffset
)
{
    auto message = std::any{};

    if (messageDescriptor.messageWithHeaderDeserializer(pendingBytes, bitsOffset, message))
    {
        LOG_DEBUG("Successfully read the message protocol ", messageIdentifier);

        eraseBits(pendingBytes, bitsOffset);

        const auto consumerPosition = inboundMessageConsumers_.find(messageIdentifier);
        if (consumerPosition != inboundMessageConsumers_.cend())
        {
            wrapInTryCatchAndInvoke(messageIdentifier, consumerPosition->second, std::move(message));
        }
        else
        {
            LOG_WARNING(
                "The message protocol with identifier ", messageIdentifier,
                " has no consumer registered and will be skipped"
            );
        }

        return true;
    }
    else
    {
        LOG_DEBUG("Buffer does not yet have enough bytes to read the message protocol ", messageIdentifier);

        return false;
    }
}

bool ProtocolReader::readInboundRequest(
    const SocketIdentifier receivingSocketIdentifier, std::vector<uint8_t>& pendingBytes,
    const ProtocolIdentifier requestIdentifier, const ProtocolDescriptor& requestDescriptor, size_t& bitsOffset
)
{
    auto sequenceNumber = ProtocolSequenceNumber{};

    auto request = std::any{};

    if (requestDescriptor.sequencedProtocolWithHeaderDeserializer(pendingBytes, bitsOffset, sequenceNumber, request))
    {
        LOG_DEBUG("Successfully read the request protocol ", requestIdentifier);

        eraseBits(pendingBytes, bitsOffset);

        const auto consumerPosition = inboundRequestConsumers_.find(requestIdentifier);
        if (consumerPosition != inboundRequestConsumers_.cend())
        {
            // There is no need to wrap the inbound request consumer inside a try/catch.
            // The inbound request consumer calls the protocol's implementation of Metadata::invokeCallback which wraps
            // the user callback inside a try/catch. If the user callback throws, then the request consumer returns an
            // std::any containing an Expected<Response> such that Expected<Response>::failure() == true.
            //
            // +----------------------------------------+
            // |        Inbound request consumer        |
            // | +------------------------------------+ |
            // | | Response::Metadata::invokeCallback | |
            // | |          +---------------+         | |
            // | |          | User callback |         | |
            // | |          +---------------+         | |
            // | +------------------------------------+ |
            // +----------------------------------------+
            //
            const auto response = consumerPosition->second(std::move(request));

            auto serializedResponse = requestDescriptor.responseWithHeaderSerializer(response, sequenceNumber);

            // If the outbound response consumer throws (i.e. the socket cannot send bytes), then the exception is
            // propagated up to the SocketContainer/OperationContainer which should erase the receiving socket.
            outboundResponseConsumer_(receivingSocketIdentifier, std::move(serializedResponse));
        }
        else
        {
            LOG_WARNING(
                "The request protocol with identifier ", requestIdentifier,
                " has no consumer registered and will be skipped"
            );
        }

        return true;
    }
    else
    {
        LOG_DEBUG("Buffer does not yet have enough bytes to read the request protocol ", requestIdentifier);

        return false;
    }
}

bool ProtocolReader::readInboundResponse(
    std::vector<uint8_t>& pendingBytes, const ProtocolIdentifier responseIdentifier,
    const ProtocolDescriptor& responseDescriptor, size_t& bitsOffset
)
{
    auto sequenceNumber = ProtocolSequenceNumber{};

    auto response = std::any{};

    if (responseDescriptor.sequencedProtocolWithHeaderDeserializer(pendingBytes, bitsOffset, sequenceNumber, response))
    {
        LOG_DEBUG("Successfully read the response protocol ", responseIdentifier);

        eraseBits(pendingBytes, bitsOffset);

        const auto consumerPosition = inboundOneShotResponseConsumers_.find(sequenceNumber);
        if (consumerPosition != inboundOneShotResponseConsumers_.cend())
        {
            SCOPE_EXIT([&]() { inboundOneShotResponseConsumers_.erase(consumerPosition); });

            wrapInTryCatchAndInvoke(responseIdentifier, consumerPosition->second, std::move(response));
        }
        else
        {
            LOG_ERROR(
                "The response protocol with identifier ", responseIdentifier, " and sequence number ", sequenceNumber,
                " has no consumer registered and will be skipped"
            );
        }

        return true;
    }
    else
    {
        LOG_DEBUG("Buffer does not yet have enough bytes to read the response protocol ", responseIdentifier);

        return false;
    }
}

void ProtocolReader::readInboundBytes(
    const SocketIdentifier receivingSocketIdentifier, std::vector<uint8_t>& pendingBytes,
    const std::span<const uint8_t> bytes
)
{
    pendingBytes.insert(pendingBytes.end(), bytes.cbegin(), bytes.cend());

    while (pendingBytes.size() >= sizeof(ProtocolIdentifier))
    {
        auto bitsOffset = size_t{0};

        const auto identifier = BinarySerializer<ProtocolIdentifier>::deserialize(pendingBytes, bitsOffset);

        // Unknown identifiers will cause getProtocolDescriptor to throw. The exception is propagated up to the
        // SocketContainer/OperationContainer which should erase the receiving socket.
        const auto descriptor = ProtocolRegistry::getGlobalInstance().getProtocolDescriptor(identifier);

        auto consumed = false;

        if (descriptor.protocolType == ProtocolType::message)
        {
            consumed = readInboundMessage(pendingBytes, identifier, descriptor, bitsOffset);
        }
        else if (descriptor.protocolType == ProtocolType::request)
        {
            consumed = readInboundRequest(receivingSocketIdentifier, pendingBytes, identifier, descriptor, bitsOffset);
        }
        else if (descriptor.protocolType == ProtocolType::response)
        {
            consumed = readInboundResponse(pendingBytes, identifier, descriptor, bitsOffset);
        }
        else
        {
            THROW(std::logic_error, "Unknown protocol type");
        }

        if (!consumed)
        {
            return;
        }
    }
}

}
