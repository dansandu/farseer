#pragma once

#include "dansandu/ballotin/binary.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/farseer/binary_serialization.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/protocol_metadata.hpp"

namespace dansandu::farseer::packet_serialization
{

template<typename Message>
std::vector<uint8_t> serializeMessagePacket(const Message& message)
{
    using dansandu::ballotin::binary::bitsPerByte;
    using dansandu::farseer::binary_serialization::BinarySerializer;
    using dansandu::farseer::protocol_metadata::ProtocolMetadata;

    auto bytes = std::vector<uint8_t>{};
    auto bitsCount = size_t{0};

    BinarySerializer<ProtocolIdentifier>::serialize(ProtocolMetadata<Message>::getProtocolIdentifier(), bytes,
                                                    bitsCount);

    if constexpr (ProtocolMetadata<Message>::hasStaticSize)
    {
        BinarySerializer<Message>::serialize(message, bytes, bitsCount);
    }
    else
    {
        auto dynamicBytes = std::vector<uint8_t>{};
        auto dynamicBitsCount = size_t{0};

        BinarySerializer<Message>::serialize(message, dynamicBytes, dynamicBitsCount);

        BinarySerializer<uint64_t>::serialize(dynamicBitsCount, bytes, bitsCount);

        if (bitsCount % bitsPerByte != 0)
        {
            THROW(std::logic_error, "bits count should be a multiple of ", bitsPerByte,
                  " otherwise there will be gaps in the packet");
        }

        bytes.insert(bytes.end(), dynamicBytes.cbegin(), dynamicBytes.cend());

        bitsCount += dynamicBitsCount;
    }

    return bytes;
}

}
