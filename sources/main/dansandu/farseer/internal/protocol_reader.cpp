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

namespace dansandu::farseer::internal::protocol_reader
{

ProtocolReader::ProtocolReader(
    UniqueFunction<void(const SocketIdentifier, std::vector<uint8_t>&&)>&& serializedResponseConsumer)
    : serializedResponseConsumer_{std::move(serializedResponseConsumer)}
{
}

void ProtocolReader::registerMessageConsumer(const ProtocolIdentifier messageIdentifier,
                                             UniqueFunction<void(std::any&&)>&& consumer)
{
    if (!ProtocolRegistry::getGlobalInstance().isProtocolRegistered(messageIdentifier))
    {
        THROW(ProtocolNotRegisteredError, "No message protocol is registered with identifier ", messageIdentifier);
    }

    if (!messageConsumers_.contains(messageIdentifier))
    {
        messageConsumers_.insert({messageIdentifier, std::move(consumer)});
    }
    else
    {
        THROW(ProtocolConsumerAlreadyRegisteredError,
              "Another protocol consumer is already registered with identifier ", messageIdentifier);
    }
}

void ProtocolReader::registerRequestConsumer(const ProtocolIdentifier requestIdentifier,
                                             UniqueFunction<std::any(std::any&&)>&& requestConsumer)
{
    if (!ProtocolRegistry::getGlobalInstance().isProtocolRegistered(requestIdentifier))
    {
        THROW(ProtocolNotRegisteredError, "No request protocol is registered with identifier ", requestIdentifier);
    }

    if (!requestConsumers_.contains(requestIdentifier))
    {
        requestConsumers_.insert({requestIdentifier, std::move(requestConsumer)});
    }
    else
    {
        THROW(ProtocolConsumerAlreadyRegisteredError,
              "Another protocol consumer is already registered with identifier ", requestIdentifier);
    }
}

void ProtocolReader::registerOneShotResponseConsumer(const ProtocolSequenceNumber sequenceNumber,
                                                     UniqueFunction<void(std::any&&)>&& responseConsumer)
{
    if (!oneShotResponseConsumers_.contains(sequenceNumber))
    {
        oneShotResponseConsumers_.insert({sequenceNumber, std::move(responseConsumer)});
    }
    else
    {
        THROW(ProtocolConsumerAlreadyRegisteredError,
              "Another response consumer is already registered with sequence number ", sequenceNumber);
    }
}

namespace
{

void eraseBits(std::vector<uint8_t>& bytes, const size_t bitsOffset)
{
    const auto numberOfBytesToErase = numberOfBitsToNumberOfBytes(bitsOffset);

    bytes.erase(bytes.cbegin(), bytes.cbegin() + numberOfBytesToErase);
}

}

void ProtocolReader::readMessage(std::vector<uint8_t>& pendingBytes, const ProtocolIdentifier messageIdentifier,
                                 const ProtocolDescriptor& messageDescriptor, size_t& bitsOffset)
{
    auto message = std::any{};

    if (messageDescriptor.messageWithHeaderDeserializer(pendingBytes, bitsOffset, message))
    {
        LOG_DEBUG("Successfully read message protocol ", messageIdentifier);

        eraseBits(pendingBytes, bitsOffset);

        const auto consumerPosition = messageConsumers_.find(messageIdentifier);
        if (consumerPosition != messageConsumers_.cend())
        {
            consumerPosition->second(std::move(message));
        }
        else
        {
            LOG_WARNING("Message protocol with identifier ", messageIdentifier,
                        " has no consumer registered and will be skipped");
        }
    }
    else
    {
        LOG_DEBUG("Buffer does not have enough bytes to read message protocol ", messageIdentifier, " just yet");
    }
}

void ProtocolReader::readRequest(const SocketIdentifier receivingSocketIdentifier, std::vector<uint8_t>& pendingBytes,
                                 const ProtocolIdentifier requestIdentifier,
                                 const ProtocolDescriptor& requestDescriptor, size_t& bitsOffset)
{
    auto sequenceNumber = ProtocolSequenceNumber{};

    auto request = std::any{};

    if (requestDescriptor.sequencedProtocolWithHeaderDeserializer(pendingBytes, bitsOffset, sequenceNumber, request))
    {
        LOG_DEBUG("Successfully read request protocol ", requestIdentifier);

        eraseBits(pendingBytes, bitsOffset);

        const auto consumerPosition = requestConsumers_.find(requestIdentifier);
        if (consumerPosition != requestConsumers_.cend())
        {
            const auto response = consumerPosition->second(std::move(request));

            auto serializedResponse = requestDescriptor.responseWithHeaderSerializer(response, sequenceNumber);

            serializedResponseConsumer_(receivingSocketIdentifier, std::move(serializedResponse));
        }
        else
        {
            LOG_WARNING("Request protocol with identifier ", requestIdentifier,
                        " has no consumer registered and will be skipped");
        }
    }
    else
    {
        LOG_DEBUG("Buffer does not have enough bytes to read request protocol ", requestIdentifier, " just yet");
    }
}

void ProtocolReader::readResponse(std::vector<uint8_t>& pendingBytes, const ProtocolIdentifier responseIdentifier,
                                  const ProtocolDescriptor& responseDescriptor, size_t& bitsOffset)
{
    auto sequenceNumber = ProtocolSequenceNumber{};

    auto response = std::any{};

    if (responseDescriptor.sequencedProtocolWithHeaderDeserializer(pendingBytes, bitsOffset, sequenceNumber, response))
    {
        LOG_DEBUG("Successfully read response protocol ", responseIdentifier);

        eraseBits(pendingBytes, bitsOffset);

        const auto consumerPosition = oneShotResponseConsumers_.find(sequenceNumber);
        if (consumerPosition != oneShotResponseConsumers_.cend())
        {
            SCOPE_EXIT([&]() { oneShotResponseConsumers_.erase(consumerPosition); });

            consumerPosition->second(std::move(response));
        }
        else
        {
            LOG_ERROR("Response protocol with identifier ", responseIdentifier, " and sequence number ", sequenceNumber,
                      " has no consumer registered and will be skipped");
        }
    }
    else
    {
        LOG_DEBUG("Buffer does not have enough bytes to read response protocol ", responseIdentifier, " just yet");
    }
}

void ProtocolReader::read(const SocketIdentifier receivingSocketIdentifier, std::vector<uint8_t>& pendingBytes,
                          const std::span<const uint8_t> bytes)
{
    pendingBytes.insert(pendingBytes.end(), bytes.cbegin(), bytes.cend());

    if (pendingBytes.size() < sizeof(ProtocolIdentifier))
    {
        LOG_DEBUG("Buffer does not have enough bytes to read protocol identifier just yet");
        return;
    }

    auto bitsOffset = size_t{0};

    const auto identifier = BinarySerializer<ProtocolIdentifier>::deserialize(pendingBytes, bitsOffset);

    const auto descriptor = ProtocolRegistry::getGlobalInstance().getProtocolDescriptor(identifier);

    if (descriptor.protocolType == ProtocolType::message)
    {
        readMessage(pendingBytes, identifier, descriptor, bitsOffset);
    }
    else if (descriptor.protocolType == ProtocolType::request)
    {
        readRequest(receivingSocketIdentifier, pendingBytes, identifier, descriptor, bitsOffset);
    }
    else if (descriptor.protocolType == ProtocolType::response)
    {
        readResponse(pendingBytes, identifier, descriptor, bitsOffset);
    }
    else
    {
        THROW(std::logic_error, "Unknown protocol type");
    }
}

}
