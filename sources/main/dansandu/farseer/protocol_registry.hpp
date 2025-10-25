#pragma once

#include "dansandu/ballotin/exception.hpp"
#include "dansandu/farseer/binary_serialization.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/protocol_metadata.hpp"

#include <any>
#include <map>
#include <mutex>

namespace dansandu::farseer::protocol_registry
{

class PRALINE_EXPORT ProtocolRegistry
{
public:
    using DeserializerType = std::any (*)(const std::vector<uint8_t>& bytes, size_t& offset);

    struct Entry
    {
        DeserializerType deserializer;
        uint64_t numberOfBits;
        ProtocolIdentifier identifier;
        bool hasStaticSize;
    };

    static ProtocolRegistry& getGlobalInstance();

    ProtocolRegistry() = default;

    ProtocolRegistry(const ProtocolRegistry& other) = delete;
    ProtocolRegistry(ProtocolRegistry&& other) noexcept = delete;
    ProtocolRegistry& operator=(const ProtocolRegistry& other) = delete;
    ProtocolRegistry& operator=(ProtocolRegistry&& other) noexcept = delete;

    template<typename T>
    int registerProtocol()
    {
        using ProtocolMetadataType = dansandu::farseer::protocol_metadata::ProtocolMetadata<T>;
        using BinarySerializerType = dansandu::farseer::binary_serialization::BinarySerializer<T>;

        const auto protocolIdentifier = ProtocolMetadataType::getProtocolIdentifier();

        const auto lock = std::lock_guard<std::mutex>{mutex_};
        const auto [position, inserted] = entries_.insert(
            {protocolIdentifier, Entry{
                                     .deserializer = [](const std::vector<uint8_t>& bytes, size_t& offset)
                                     { return std::any(BinarySerializerType::deserialize(bytes, offset)); },
                                     .numberOfBits = ProtocolMetadataType::numberOfBits,
                                     .identifier = protocolIdentifier,
                                     .hasStaticSize = ProtocolMetadataType::hasStaticSize,
                                 }});

        if (!inserted)
        {
            using dansandu::farseer::exception::ProtocolIdentifierAlreadyRegisteredError;
            THROW(ProtocolIdentifierAlreadyRegisteredError, "a protocol is already registered with identifier '",
                  protocolIdentifier, "'");
        }

        return 0;
    }

    const Entry& getProtocol(const ProtocolIdentifier identifier) const;

private:
    std::map<ProtocolIdentifier, Entry> entries_;
    mutable std::mutex mutex_;
};

}
