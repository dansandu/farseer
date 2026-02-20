#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/ballotin/binary.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/ballotin/scope.hpp"
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
    Function<void(const SocketServiceId, std::vector<uint8_t>&&)>&& serializedExpectedResponseConsumer)
    : serializedExpectedResponseConsumer_{std::move(serializedExpectedResponseConsumer)}
{
}

void ProtocolReader::registerMessageConsumer(const ProtocolIdentifier messageIdentifier,
                                             Function<void(std::any&&)>&& consumer)
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
                                             Function<std::any(std::any&&)>&& requestConsumer)
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

void ProtocolReader::registerOneShotExpectedResponseConsumer(const ProtocolSequenceNumber sequenceNumber,
                                                             Function<void(std::any&&)>&& expectedResponseConsumer)
{
    if (!oneShotExpectedResponseConsumers_.contains(sequenceNumber))
    {
        oneShotExpectedResponseConsumers_.insert({sequenceNumber, std::move(expectedResponseConsumer)});
    }
    else
    {
        THROW(ProtocolConsumerAlreadyRegisteredError,
              "Another response consumer is already registered with sequence number ", sequenceNumber);
    }
}

void ProtocolReader::eraseBits(const size_t bitsOffset)
{
    const auto numberOfBytesToErase = numberOfBitsToNumberOfBytes(bitsOffset);

    buffer_.erase(buffer_.cbegin(), buffer_.cbegin() + numberOfBytesToErase);
}

void ProtocolReader::readMessage(const ProtocolIdentifier messageIdentifier,
                                 const ProtocolDescriptor& messageDescriptor, size_t& bitsOffset)
{
    auto sequenceNumber = ProtocolSequenceNumber{};

    auto message = std::any{};

    if (messageDescriptor.protocolDeserializer(buffer_, bitsOffset, sequenceNumber, message))
    {
        LOG_DEBUG("Successfully read message protocol ", messageIdentifier.getUnderlying());

        eraseBits(bitsOffset);

        const auto consumerPosition = messageConsumers_.find(messageIdentifier);
        if (consumerPosition != messageConsumers_.cend())
        {
            consumerPosition->second(std::move(message));
        }
        else
        {
            LOG_WARNING("Message protocol with identifier ", messageIdentifier.getUnderlying(),
                        " has no consumer registered and will be skipped");
        }
    }
    else
    {
        LOG_DEBUG("Buffer does not have enough bytes to read message protocol ", messageIdentifier.getUnderlying(),
                  " just yet");
    }
}

void ProtocolReader::readRequest(const SocketServiceId receiverSocketServiceId,
                                 const ProtocolIdentifier requestIdentifier,
                                 const ProtocolDescriptor& requestDescriptor, size_t& bitsOffset)
{
    auto sequenceNumber = ProtocolSequenceNumber{};

    auto request = std::any{};

    if (requestDescriptor.protocolDeserializer(buffer_, bitsOffset, sequenceNumber, request))
    {
        LOG_DEBUG("Successfully read request protocol ", requestIdentifier.getUnderlying());

        eraseBits(bitsOffset);

        const auto consumerPosition = requestConsumers_.find(requestIdentifier);
        if (consumerPosition != requestConsumers_.cend())
        {
            const auto expectedResponse = consumerPosition->second(std::move(request));

            auto serializedExpectedResponse =
                requestDescriptor.expectedResponseSerializer(expectedResponse, sequenceNumber);

            serializedExpectedResponseConsumer_(receiverSocketServiceId, std::move(serializedExpectedResponse));
        }
        else
        {
            LOG_WARNING("Request protocol with identifier ", requestIdentifier.getUnderlying(),
                        " has no consumer registered and will be skipped");
        }
    }
    else
    {
        LOG_DEBUG("Buffer does not have enough bytes to read request protocol ", requestIdentifier.getUnderlying(),
                  " just yet");
    }
}

void ProtocolReader::readExpectedResponse(const ProtocolIdentifier responseIdentifier,
                                          const ProtocolDescriptor& responseDescriptor, size_t& bitsOffset)
{
    auto sequenceNumber = ProtocolSequenceNumber{};

    auto expectedResponse = std::any{};

    if (responseDescriptor.protocolDeserializer(buffer_, bitsOffset, sequenceNumber, expectedResponse))
    {
        LOG_DEBUG("Successfully read expected response protocol ", responseIdentifier.getUnderlying());

        eraseBits(bitsOffset);

        const auto consumerPosition = oneShotExpectedResponseConsumers_.find(sequenceNumber);
        if (consumerPosition != oneShotExpectedResponseConsumers_.cend())
        {
            SCOPE_EXIT([&]() { oneShotExpectedResponseConsumers_.erase(consumerPosition); });

            consumerPosition->second(std::move(expectedResponse));
        }
        else
        {
            LOG_ERROR("Response protocol with identifier ", responseIdentifier.getUnderlying(), " and sequence number ",
                      sequenceNumber.getUnderlying(), " has no consumer registered and will be skipped");
        }
    }
    else
    {
        LOG_DEBUG("Buffer does not have enough bytes to read expected response protocol ",
                  responseIdentifier.getUnderlying(), " just yet");
    }
}

void ProtocolReader::read(const SocketServiceId receiverSocketServiceId, const std::span<const uint8_t> bytes)
{
    buffer_.insert(buffer_.end(), bytes.cbegin(), bytes.cend());

    if (buffer_.size() < sizeof(ProtocolIdentifier))
    {
        LOG_DEBUG("Buffer does not have enough bytes to read protocol identifier just yet");
        return;
    }

    auto bitsOffset = size_t{0};

    const auto identifier = BinarySerializer<ProtocolIdentifier>::deserialize(buffer_, bitsOffset);

    const auto descriptor = ProtocolRegistry::getGlobalInstance().getProtocolDescriptor(identifier);

    if (descriptor.protocolType == ProtocolType::message)
    {
        readMessage(identifier, descriptor, bitsOffset);
    }
    else if (descriptor.protocolType == ProtocolType::request)
    {
        readRequest(receiverSocketServiceId, identifier, descriptor, bitsOffset);
    }
    else if (descriptor.protocolType == ProtocolType::response)
    {
        readExpectedResponse(identifier, descriptor, bitsOffset);
    }
    else
    {
        THROW(std::logic_error, "Unknown protocol type");
    }
}

}
