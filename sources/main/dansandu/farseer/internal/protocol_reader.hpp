#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/protocol_registry.hpp"

#include <any>
#include <map>

namespace dansandu::farseer::internal::protocol_reader
{

class ProtocolReader
{
public:
    ProtocolReader() = default;

    void registerInboundMessageConsumer(
        const ProtocolIdentifier messageIdentifier, UniqueFunction<void(std::any&&)>&& messageConsumer
    );

    void registerInboundRequestConsumer(
        const ProtocolIdentifier requestIdentifier, UniqueFunction<std::any(std::any&&)>&& requestConsumer
    );

    void registerInboundOneShotResponseConsumer(
        const ProtocolSequenceNumber sequenceNumber, UniqueFunction<void(std::any&&)>&& responseConsumer
    );

    // The inbound and outbound byte buffers belong to the receiving socket so that sockets sharing this reader (i.e.
    // the accepted sockets of a listening socket) don't interleave their byte streams.
    //
    // Responses are appended to the outbound bytes and should be sent by the caller once this function returns, so
    // that all responses to a single read are sent together. If reading a protocol throws (e.g. an unknown
    // identifier), the responses already appended are not sent; the stream is corrupt at that point and the
    // receiving socket is erased.
    void readInboundBytes(std::vector<uint8_t>& inboundBytes, std::vector<uint8_t>& outboundBytes);

private:
    bool readInboundMessage(
        const ProtocolIdentifier messageIdentifier,
        const dansandu::farseer::protocol_registry::ProtocolDescriptor& messageDescriptor,
        std::vector<uint8_t>& inboundBytes, size_t& inboundBitsOffset
    );

    bool readInboundRequest(
        const ProtocolIdentifier requestIdentifier,
        const dansandu::farseer::protocol_registry::ProtocolDescriptor& requestDescriptor,
        std::vector<uint8_t>& inboundBytes, size_t& inboundBitsOffset, std::vector<uint8_t>& outboundBytes
    );

    bool readInboundResponse(
        const ProtocolIdentifier responseIdentifier,
        const dansandu::farseer::protocol_registry::ProtocolDescriptor& responseDescriptor,
        std::vector<uint8_t>& inboundBytes, size_t& inboundBitsOffset
    );

    std::map<ProtocolIdentifier, UniqueFunction<void(std::any&&)>> inboundMessageConsumers_;
    std::map<ProtocolIdentifier, UniqueFunction<std::any(std::any&&)>> inboundRequestConsumers_;
    std::map<ProtocolSequenceNumber, UniqueFunction<void(std::any&&)>> inboundOneShotResponseConsumers_;
};

}
