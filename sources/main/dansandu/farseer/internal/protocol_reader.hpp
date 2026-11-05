#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/protocol_registry.hpp"

#include <any>
#include <chrono>
#include <map>
#include <optional>

namespace dansandu::farseer::internal::protocol_reader
{

struct ResponseEntry
{
    ProtocolIdentifier responseIdentifier;
    UniqueFunction<void(std::any&&)> responseConsumer;
    FailedResponseProducer failedResponseProducer;
    std::optional<std::chrono::steady_clock::time_point> deadline;
};

class ProtocolReader
{
public:
    ProtocolReader() = default;

    ProtocolReader(const ProtocolReader&) = delete;

    ProtocolReader(ProtocolReader&& other) noexcept;

    ProtocolReader& operator=(const ProtocolReader&) = delete;

    // Pending response callbacks of the assigned-to reader are aborted before taking over the other reader's state.
    ProtocolReader& operator=(ProtocolReader&& other) noexcept;

    ~ProtocolReader() noexcept;

    void registerInboundMessageConsumer(
        const ProtocolIdentifier messageIdentifier, UniqueFunction<void(std::any&&)>&& messageConsumer
    );

    void registerInboundRequestConsumer(
        const ProtocolIdentifier requestIdentifier, UniqueFunction<std::any(std::any&&)>&& requestConsumer
    );

    void registerInboundOneShotResponseConsumer(
        const ProtocolIdentifier responseIdentifier, const ProtocolSequenceNumber sequenceNumber,
        const std::optional<std::chrono::steady_clock::time_point> deadline,
        UniqueFunction<void(std::any&&)>&& responseConsumer
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
    void readInboundBytes(std::vector<uint8_t>& inboundBytes, std::vector<uint8_t>& outboundBytes);

    std::optional<std::chrono::steady_clock::time_point> getEarliestResponseDeadline() const noexcept;

    void eraseTimedOutResponses() noexcept;

private:
    void abortPendingResponses() noexcept;

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
        const std::vector<uint8_t>& inboundBytes, size_t& inboundBitsOffset
    );

    std::map<ProtocolIdentifier, UniqueFunction<void(std::any&&)>> inboundMessageConsumers_;
    std::map<ProtocolIdentifier, UniqueFunction<std::any(std::any&&)>> inboundRequestConsumers_;
    std::map<ProtocolSequenceNumber, ResponseEntry> inboundOneShotResponseConsumers_;
};

}
