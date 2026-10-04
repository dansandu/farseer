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
        UniqueFunction<void(const SocketIdentifier, std::vector<uint8_t>&&)>&& outboundResponseConsumer
    );

    void registerInboundMessageConsumer(
        const ProtocolIdentifier messageIdentifier, UniqueFunction<void(std::any&&)>&& messageConsumer
    );

    void registerInboundRequestConsumer(
        const ProtocolIdentifier requestIdentifier, UniqueFunction<std::any(std::any&&)>&& requestConsumer
    );

    void registerInboundOneShotResponseConsumer(
        const ProtocolSequenceNumber sequenceNumber, UniqueFunction<void(std::any&&)>&& responseConsumer
    );

    // The pending bytes buffer belongs to the receiving socket so that sockets sharing this reader (i.e. the
    // accepted sockets of a listening socket) don't interleave their byte streams.
    void readInboundBytes(
        const SocketIdentifier receivingSocketIdentifier, std::vector<uint8_t>& pendingBytes,
        const std::span<const uint8_t> bytes
    );

private:
    bool readInboundMessage(
        std::vector<uint8_t>& pendingBytes, const ProtocolIdentifier messageIdentifier,
        const dansandu::farseer::protocol_registry::ProtocolDescriptor& messageDescriptor, size_t& bitsOffset
    );

    bool readInboundRequest(
        const SocketIdentifier receivingSocketIdentifier, std::vector<uint8_t>& pendingBytes,
        const ProtocolIdentifier requestIdentifier,
        const dansandu::farseer::protocol_registry::ProtocolDescriptor& requestDescriptor, size_t& bitsOffset
    );

    bool readInboundResponse(
        std::vector<uint8_t>& pendingBytes, const ProtocolIdentifier responseIdentifier,
        const dansandu::farseer::protocol_registry::ProtocolDescriptor& responseDescriptor, size_t& bitsOffset
    );

    UniqueFunction<void(const SocketIdentifier, std::vector<uint8_t>&&)> outboundResponseConsumer_;
    std::map<ProtocolIdentifier, UniqueFunction<void(std::any&&)>> inboundMessageConsumers_;
    std::map<ProtocolIdentifier, UniqueFunction<std::any(std::any&&)>> inboundRequestConsumers_;
    std::map<ProtocolSequenceNumber, UniqueFunction<void(std::any&&)>> inboundOneShotResponseConsumers_;
};

}
