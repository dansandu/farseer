#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/protocol_registry.hpp"

#include <any>
#include <map>
#include <span>

namespace dansandu::farseer::internal::protocol_reader
{

class ProtocolReader
{
public:
    explicit ProtocolReader(
        UniqueFunction<void(const SocketIdentifier, std::vector<uint8_t>&&)>&& serializedResponseConsumer);

    void registerMessageConsumer(const ProtocolIdentifier messageIdentifier,
                                 UniqueFunction<void(std::any&&)>&& messageConsumer);

    void registerRequestConsumer(const ProtocolIdentifier requestIdentifier,
                                 UniqueFunction<std::any(std::any&&)>&& requestConsumer);

    void registerOneShotResponseConsumer(const ProtocolSequenceNumber sequenceNumber,
                                         UniqueFunction<void(std::any&&)>&& responseConsumer);

    void read(const SocketIdentifier receivingSocketIdentifier, const std::span<const uint8_t> bytes);

private:
    void eraseBits(const size_t bitsOffset);

    void readMessage(const ProtocolIdentifier messageIdentifier,
                     const dansandu::farseer::protocol_registry::ProtocolDescriptor& messageDescriptor,
                     size_t& bitsOffset);

    void readRequest(const SocketIdentifier receivingSocketIdentifier, const ProtocolIdentifier requestIdentifier,
                     const dansandu::farseer::protocol_registry::ProtocolDescriptor& requestDescriptor,
                     size_t& bitsOffset);

    void readResponse(const ProtocolIdentifier responseIdentifier,
                      const dansandu::farseer::protocol_registry::ProtocolDescriptor& responseDescriptor,
                      size_t& bitsOffset);

    UniqueFunction<void(const SocketIdentifier, std::vector<uint8_t>&&)> serializedResponseConsumer_;
    std::map<ProtocolIdentifier, UniqueFunction<void(std::any&&)>> messageConsumers_;
    std::map<ProtocolIdentifier, UniqueFunction<std::any(std::any&&)>> requestConsumers_;
    std::map<ProtocolSequenceNumber, UniqueFunction<void(std::any&&)>> oneShotResponseConsumers_;
    std::vector<uint8_t> buffer_;
};

}
