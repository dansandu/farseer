#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/ballotin/binary.hpp"
#include "dansandu/farseer/binary_serialization.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/protocol_registry.hpp"
#include "dansandu/journey/logging.hpp"

#include <cstdint>

using dansandu::ballotin::binary::bitsPerByte;
using dansandu::farseer::binary_serialization::BinarySerializer;
using dansandu::farseer::exception::ProtocolConsumerAlreadyRegisteredError;
using dansandu::farseer::protocol_registry::ProtocolRegistry;

namespace dansandu::farseer::internal::protocol_reader
{

void ProtocolReader::registerProtocolConsumer(ProtocolIdentifier protocolIdentifier,
                                              ProtocolConsumerType protocolConsumer)
{
    const auto position = protocolConsumers_.find(protocolIdentifier);
    if (position == protocolConsumers_.cend())
    {
        protocolConsumers_.insert({protocolIdentifier, std::move(protocolConsumer)});
    }
    else
    {
        THROW(ProtocolConsumerAlreadyRegisteredError, "a protocol consumer is already registered with identifier '",
              protocolIdentifier, "'");
    }
}

void ProtocolReader::read(const std::span<const uint8_t> bytes)
{
    buffer_.insert(buffer_.end(), bytes.cbegin(), bytes.cend());

    if (buffer_.size() < sizeof(ProtocolIdentifier))
    {
        return;
    }

    auto bitsOffset = size_t{0};

    const auto protocolIdentifier = BinarySerializer<ProtocolIdentifier>::deserialize(buffer_, bitsOffset);

    const auto& protocol = ProtocolRegistry::getGlobalInstance().getProtocol(protocolIdentifier);

    const auto consumerPosition = protocolConsumers_.find(protocolIdentifier);

    if (protocol.hasStaticSize)
    {
        if (bitsPerByte * buffer_.size() >= bitsOffset + protocol.numberOfBits)
        {
            auto message = protocol.deserializer(buffer_, bitsOffset);

            const auto bytesToErase = bitsOffset / bitsPerByte + (bitsOffset % bitsPerByte > 0);
            buffer_.erase(buffer_.cbegin(), buffer_.cbegin() + bytesToErase);

            if (consumerPosition != protocolConsumers_.cend())
            {
                consumerPosition->second(std::move(message));
            }
            else
            {
                LOG_WARNING("message with identifier '", protocolIdentifier.toString(),
                            "' has no consumer registered and will be skipped");
            }
        }
    }
    else
    {
        if (bitsPerByte * buffer_.size() >= bitsOffset + bitsPerByte * sizeof(uint64_t))
        {
            const auto protocolBitsCount = BinarySerializer<uint64_t>::deserialize(buffer_, bitsOffset);

            if (bitsPerByte * buffer_.size() >= bitsOffset + protocolBitsCount)
            {
                auto message = protocol.deserializer(buffer_, bitsOffset);

                const auto bytesToErase = bitsOffset / bitsPerByte + (bitsOffset % bitsPerByte > 0);
                buffer_.erase(buffer_.cbegin(), buffer_.cbegin() + bytesToErase);

                if (consumerPosition != protocolConsumers_.cend())
                {
                    consumerPosition->second(std::move(message));
                }
                else
                {
                    LOG_WARNING("message with identifier '", protocolIdentifier.toString(),
                                "' has no consumer registered and will be skipped");
                }
            }
        }
    }
}

}
