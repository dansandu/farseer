#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/response_consumer_container.hpp"
#include "dansandu/farseer/protocol_registry.hpp"

#include <any>
#include <map>

namespace dansandu::farseer::internal::protocol_reader
{

class ProtocolReader
{
public:
    void registerInboundMessageConsumer(
        const ProtocolIdentifier messageIdentifier, UniqueFunction<void(std::any&&)>&& messageConsumer
    );

    void registerInboundRequestConsumer(
        const ProtocolIdentifier requestIdentifier, UniqueFunction<std::any(std::any&&)>&& requestConsumer
    );

    // The inbound and outbound byte buffers belong to the receiving socket so that sockets sharing this reader (i.e.
    // the accepted sockets of a listening socket) don't interleave their byte streams.
    //
    // If inboundBytes doesn't hold enough bytes to parse an entire protocol from the front, then the bytes are kept
    // until the entire protocol can be read.
    //
    // Responses are appended to the outbound bytes and should be sent by the caller once this function returns, so
    // that all responses to a single read are sent together.
    //
    // If reading a protocol throws (e.g. an unknown identifier), the responses already appended are not sent; both the
    // inbound and outbound streams are corrupt at that point and the receiving socket is erased.
    //
    // Reads messages and requests using the consumers of this reader and responses using the consumers of
    // responseConsumerContainer. An accepted socket shares the message and request consumers of its listening socket,
    // but the responses it is waiting for are its own so that they are aborted when the accepted socket is erased.
    void readInboundBytes(
        std::vector<uint8_t>& inboundBytes, std::vector<uint8_t>& outboundBytes,
        dansandu::farseer::internal::response_consumer_container::ResponseConsumerContainer& responseConsumerContainer
    );

private:
    bool readInboundMessage(
        const ProtocolIdentifier messageIdentifier,
        const dansandu::farseer::protocol_registry::ProtocolDescriptor& messageDescriptor,
        const std::vector<uint8_t>& inboundBytes, size_t& inboundBitsOffset
    );

    bool readInboundRequest(
        const ProtocolIdentifier requestIdentifier,
        const dansandu::farseer::protocol_registry::ProtocolDescriptor& requestDescriptor,
        const std::vector<uint8_t>& inboundBytes, size_t& inboundBitsOffset, std::vector<uint8_t>& outboundBytes
    );

    bool readInboundResponse(
        const ProtocolIdentifier responseIdentifier,
        const dansandu::farseer::protocol_registry::ProtocolDescriptor& responseDescriptor,
        const std::vector<uint8_t>& inboundBytes, size_t& inboundBitsOffset,
        dansandu::farseer::internal::response_consumer_container::ResponseConsumerContainer& responseConsumerContainer
    );

    std::map<ProtocolIdentifier, UniqueFunction<void(std::any&&)>> inboundMessageConsumers_;
    std::map<ProtocolIdentifier, UniqueFunction<std::any(std::any&&)>> inboundRequestConsumers_;
};

}
