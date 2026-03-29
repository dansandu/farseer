#pragma once

#include "dansandu/ballotin/binary.hpp"
#include "dansandu/farseer/binary_serialization.hpp"
#include "dansandu/farseer/common.hpp"

#include <any>
#include <vector>

namespace dansandu::farseer::protocol_serialization
{

template<typename Protocol>
void serializeDynamicProtocol(const Protocol& protocol, std::vector<uint8_t>& bytes, size_t& bitsOffset)
{
    using dansandu::ballotin::binary::bitsPerByte;
    using dansandu::ballotin::binary::pushBitsMostSignificant;
    using dansandu::farseer::binary_serialization::BinarySerializer;

    auto dynamicBytes = std::vector<uint8_t>{};
    auto dynamicBitsOffset = size_t{0};

    BinarySerializer<Protocol>::serialize(protocol, dynamicBytes, dynamicBitsOffset);

    BinarySerializer<ProtocolSize>::serialize(getProtocolSizeFromStdSize(dynamicBitsOffset), bytes, bitsOffset);

    for (const auto byte : dynamicBytes)
    {
        pushBitsMostSignificant(bytes, bitsOffset, byte, bitsPerByte);
    }
}

template<typename Message>
std::vector<uint8_t> serializeMessageProtocol(const Message& message)
{
    using dansandu::farseer::binary_serialization::BinarySerializer;

    auto bytes = std::vector<uint8_t>{};
    auto bitsOffset = size_t{0};

    BinarySerializer<ProtocolIdentifier>::serialize(Message::Metadata::getProtocolIdentifier(), bytes, bitsOffset);

    if constexpr (Message::Metadata::hasStaticSize)
    {
        BinarySerializer<Message>::serialize(message, bytes, bitsOffset);
    }
    else
    {
        serializeDynamicProtocol(message, bytes, bitsOffset);
    }

    return bytes;
}

template<typename Request>
std::vector<uint8_t> serializeRequestProtocol(const Request& request, const ProtocolSequenceNumber sequenceNumber)
{
    using dansandu::farseer::binary_serialization::BinarySerializer;

    auto bytes = std::vector<uint8_t>{};
    auto bitsOffset = size_t{0};

    BinarySerializer<ProtocolIdentifier>::serialize(Request::Metadata::getProtocolIdentifier(), bytes, bitsOffset);

    BinarySerializer<ProtocolSequenceNumber>::serialize(sequenceNumber, bytes, bitsOffset);

    if constexpr (Request::Metadata::hasStaticSize)
    {
        BinarySerializer<Request>::serialize(request, bytes, bitsOffset);
    }
    else
    {
        serializeDynamicProtocol(request, bytes, bitsOffset);
    }

    return bytes;
}

template<typename Response>
std::vector<uint8_t> serializeExpectedResponseProtocol(const std::any& expectedResponse,
                                                       const ProtocolSequenceNumber sequenceNumber)
{
    using dansandu::farseer::binary_serialization::BinarySerializer;

    const auto& casted = std::any_cast<const Expected<Response>&>(expectedResponse);

    auto bytes = std::vector<uint8_t>{};
    auto bitsOffset = size_t{0};

    BinarySerializer<ProtocolIdentifier>::serialize(Response::Metadata::getProtocolIdentifier(), bytes, bitsOffset);

    BinarySerializer<ProtocolSequenceNumber>::serialize(sequenceNumber, bytes, bitsOffset);

    serializeDynamicProtocol(casted, bytes, bitsOffset);

    return bytes;
}

template<typename Message>
bool tryDeserializeMessageProtocol(const std::vector<uint8_t>& bytes, size_t& bitsOffset, ProtocolSequenceNumber&,
                                   std::any& message)
{
    using dansandu::ballotin::binary::bitsPerByte;
    using dansandu::farseer::binary_serialization::BinarySerializer;

    if constexpr (Message::Metadata::hasStaticSize)
    {
        if (bitsPerByte * bytes.size() >= bitsOffset + Message::Metadata::staticNumberOfBits.getUnderlying())
        {
            message = BinarySerializer<Message>::deserialize(bytes, bitsOffset);
            return true;
        }
    }
    else
    {
        if (bitsPerByte * bytes.size() >= bitsOffset + bitsPerByte * sizeof(ProtocolSize))
        {
            const auto dynamicNumberOfBits = BinarySerializer<ProtocolSize>::deserialize(bytes, bitsOffset);

            if (bitsPerByte * bytes.size() >= bitsOffset + dynamicNumberOfBits.getUnderlying())
            {
                message = BinarySerializer<Message>::deserialize(bytes, bitsOffset);
                return true;
            }
        }
    }
    return false;
}

template<typename Request>
bool tryDeserializeRequestProtocol(const std::vector<uint8_t>& bytes, size_t& bitsOffset,
                                   ProtocolSequenceNumber& sequenceNumber, std::any& request)
{
    using dansandu::ballotin::binary::bitsPerByte;
    using dansandu::farseer::binary_serialization::BinarySerializer;

    if constexpr (Request::Metadata::hasStaticSize)
    {
        if (bitsPerByte * bytes.size() >= bitsOffset + bitsPerByte * sizeof(ProtocolSequenceNumber) +
                                              Request::Metadata::staticNumberOfBits.getUnderlying())
        {
            sequenceNumber = BinarySerializer<ProtocolSequenceNumber>::deserialize(bytes, bitsOffset);
            request = BinarySerializer<Request>::deserialize(bytes, bitsOffset);
            return true;
        }
    }
    else
    {
        if (bitsPerByte * bytes.size() >=
            bitsOffset + bitsPerByte * sizeof(ProtocolSequenceNumber) + bitsPerByte * sizeof(ProtocolSize))
        {
            sequenceNumber = BinarySerializer<ProtocolSequenceNumber>::deserialize(bytes, bitsOffset);

            const auto dynamicNumberOfBits = BinarySerializer<ProtocolSize>::deserialize(bytes, bitsOffset);

            if (bitsPerByte * bytes.size() >= bitsOffset + dynamicNumberOfBits.getUnderlying())
            {
                request = BinarySerializer<Request>::deserialize(bytes, bitsOffset);
                return true;
            }
        }
    }
    return false;
}

template<typename Response>
bool tryDeserializeExpectedResponseProtocol(const std::vector<uint8_t>& bytes, size_t& bitsOffset,
                                            ProtocolSequenceNumber& sequenceNumber, std::any& expectedResponse)
{
    using dansandu::ballotin::binary::bitsPerByte;
    using dansandu::farseer::binary_serialization::BinarySerializer;

    if (bitsPerByte * bytes.size() >=
        bitsOffset + bitsPerByte * sizeof(ProtocolSequenceNumber) + bitsPerByte * sizeof(ProtocolSize))
    {
        sequenceNumber = BinarySerializer<ProtocolSequenceNumber>::deserialize(bytes, bitsOffset);

        const auto dynamicNumberOfBits = BinarySerializer<ProtocolSize>::deserialize(bytes, bitsOffset);

        if (bitsPerByte * bytes.size() >= bitsOffset + dynamicNumberOfBits.getUnderlying())
        {
            expectedResponse = BinarySerializer<Expected<Response>>::deserialize(bytes, bitsOffset);
            return true;
        }
    }

    return false;
}

}
