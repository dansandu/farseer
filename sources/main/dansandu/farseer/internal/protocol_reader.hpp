#pragma once

#include "dansandu/farseer/common.hpp"

#include <any>
#include <map>
#include <span>

namespace dansandu::farseer::internal::protocol_reader
{

class ProtocolReader
{
public:
    using ProtocolConsumerType = std::function<void(std::any protocol)>;

    void registerProtocolConsumer(ProtocolIdentifier protocolIdentifier, ProtocolConsumerType protocolConsumer);

    void read(const std::span<const uint8_t> bytes);

private:
    std::vector<uint8_t> buffer_;
    std::map<ProtocolIdentifier, ProtocolConsumerType> protocolConsumers_;
};

}
