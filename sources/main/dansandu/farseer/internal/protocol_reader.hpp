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
        Function<void(const SocketServiceId, std::vector<uint8_t>&&)>&& serializedExpectedResponseConsumer);

    void registerMessageConsumer(const ProtocolIdentifier messageIdentifier,
                                 Function<void(std::any&&)>&& messageConsumer);

    void registerRequestConsumer(const ProtocolIdentifier requestIdentifier,
                                 Function<std::any(std::any&&)>&& requestConsumer);

    void registerOneShotExpectedResponseConsumer(const ProtocolSequenceNumber sequenceNumber,
                                                 Function<void(std::any&&)>&& expectedResponseConsumer);

    void read(const SocketServiceId receiverSocketServiceId, const std::span<const uint8_t> bytes);

private:
    void eraseBits(const size_t bitsOffset);

    void readMessage(const ProtocolIdentifier messageIdentifier,
                     const dansandu::farseer::protocol_registry::ProtocolDescriptor& messageDescriptor,
                     size_t& bitsOffset);

    void readRequest(const SocketServiceId receiverSocketServiceId, const ProtocolIdentifier requestIdentifier,
                     const dansandu::farseer::protocol_registry::ProtocolDescriptor& requestDescriptor,
                     size_t& bitsOffset);

    void readExpectedResponse(const ProtocolIdentifier responseIdentifier,
                              const dansandu::farseer::protocol_registry::ProtocolDescriptor& responseDescriptor,
                              size_t& bitsOffset);

    Function<void(const SocketServiceId, std::vector<uint8_t>&&)> serializedExpectedResponseConsumer_;
    std::map<ProtocolIdentifier, Function<void(std::any&&)>> messageConsumers_;
    std::map<ProtocolIdentifier, Function<std::any(std::any&&)>> requestConsumers_;
    std::map<ProtocolSequenceNumber, Function<void(std::any&&)>> oneShotExpectedResponseConsumers_;
    std::vector<uint8_t> buffer_;
};

}
