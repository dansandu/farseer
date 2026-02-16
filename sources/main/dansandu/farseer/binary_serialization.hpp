#pragma once

#include "dansandu/ballotin/binary.hpp"
#include "dansandu/ballotin/type_traits.hpp"
#include "dansandu/farseer/common.hpp"

#include <concepts>
#include <cstdint>
#include <string>
#include <vector>

namespace dansandu::farseer::binary_serialization
{

using AssociatedUnsignedTypes =
    dansandu::ballotin::type_traits::TypeDictionary<dansandu::ballotin::type_traits::TypeEntry<int8_t, uint8_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<int16_t, uint16_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<int32_t, uint32_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<int64_t, uint64_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<uint8_t, uint8_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<uint16_t, uint16_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<uint32_t, uint32_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<uint64_t, uint64_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<char, unsigned char>>;

template<typename PrimitiveType>
concept SerializablePrimitiveType = AssociatedUnsignedTypes::containsKey<PrimitiveType>;

template<typename Protocol>
concept SerializableProtocol =
    std::is_same_v<decltype(Protocol::Metadata::deserialize), Protocol(const std::vector<uint8_t>&, size_t&)> &&
    std::is_same_v<decltype(Protocol::Metadata::serialize), void(const Protocol&, std::vector<uint8_t>&, size_t&)>;

template<typename T>
struct BinarySerializer;

template<SerializablePrimitiveType PrimitiveType>
struct BinarySerializer<PrimitiveType>
{
    using UnsignedType = AssociatedUnsignedTypes::Get<PrimitiveType>;

    static PrimitiveType deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        using dansandu::ballotin::binary::bitsPerByte;
        using dansandu::ballotin::binary::getMostSignificantBits;

        const auto bits = getMostSignificantBits(bytes, bitsOffset, bitsPerByte * sizeof(UnsignedType));
        bitsOffset += bitsPerByte * sizeof(UnsignedType);
        return static_cast<UnsignedType>(bits);
    }

    static void serialize(const PrimitiveType& value, std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        using dansandu::ballotin::binary::bitsPerByte;
        using dansandu::ballotin::binary::pushBitsMostSignificant;

        pushBitsMostSignificant(bytes, bitsOffset, static_cast<UnsignedType>(value),
                                bitsPerByte * sizeof(UnsignedType));
    }
};

template<SerializableProtocol Protocol>
struct BinarySerializer<Protocol>
{
    static Protocol deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        return Protocol::Metadata::deserialize(bytes, bitsOffset);
    }

    static void serialize(const Protocol& value, std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        Protocol::Metadata::serialize(value, bytes, bitsOffset);
    }
};

template<>
struct BinarySerializer<ProtocolIdentifier>
{
    static ProtocolIdentifier deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        return ProtocolIdentifier{
            BinarySerializer<typename ProtocolIdentifier::UnderlyingType>::deserialize(bytes, bitsOffset)};
    }

    static void serialize(const ProtocolIdentifier& value, std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        BinarySerializer<typename ProtocolIdentifier::UnderlyingType>::serialize(value.getUnderlying(), bytes,
                                                                                 bitsOffset);
    }
};

template<>
struct BinarySerializer<ProtocolSize>
{
    static ProtocolSize deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        return ProtocolSize{BinarySerializer<typename ProtocolSize::UnderlyingType>::deserialize(bytes, bitsOffset)};
    }

    static void serialize(const ProtocolSize& value, std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        BinarySerializer<typename ProtocolSize::UnderlyingType>::serialize(value.getUnderlying(), bytes, bitsOffset);
    }
};

template<>
struct BinarySerializer<ProtocolSequenceNumber>
{
    static ProtocolSequenceNumber deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        return ProtocolSequenceNumber{
            BinarySerializer<typename ProtocolSequenceNumber::UnderlyingType>::deserialize(bytes, bitsOffset)};
    }

    static void serialize(const ProtocolSequenceNumber& value, std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        BinarySerializer<typename ProtocolSequenceNumber::UnderlyingType>::serialize(value.getUnderlying(), bytes,
                                                                                     bitsOffset);
    }
};

template<>
struct BinarySerializer<bool>
{
    static bool deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        using dansandu::ballotin::binary::getMostSignificantBits;

        const auto bits = getMostSignificantBits(bytes, bitsOffset, 1);
        bitsOffset += 1;
        return static_cast<bool>(bits);
    }

    static void serialize(const bool& value, std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        using dansandu::ballotin::binary::pushBitsMostSignificant;

        pushBitsMostSignificant(bytes, bitsOffset, value, 1);
    }
};

template<>
struct BinarySerializer<std::string>
{
    static std::string deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        auto value = std::string{};

        const auto numberOfElements = BinarySerializer<ProtocolSize>::deserialize(bytes, bitsOffset);

        value.reserve(numberOfElements.getUnderlying());

        for (auto index = ProtocolSize{}; index < numberOfElements; ++index)
        {
            value.push_back(BinarySerializer<std::string::value_type>::deserialize(bytes, bitsOffset));
        }

        return value;
    }

    static void serialize(const std::string& value, std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        BinarySerializer<ProtocolSize>::serialize(getProtocolSizeFromStdSize(value.size()), bytes, bitsOffset);

        for (const auto& element : value)
        {
            BinarySerializer<std::string::value_type>::serialize(element, bytes, bitsOffset);
        }
    }
};

template<typename T>
struct BinarySerializer<std::vector<T>>
{
    static std::vector<T> deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        auto value = std::vector<T>{};

        const auto numberOfElements = BinarySerializer<ProtocolSize>::deserialize(bytes, bitsOffset);

        value.reserve(numberOfElements.getUnderlying());

        for (auto index = ProtocolSize{}; index < numberOfElements; ++index)
        {
            value.push_back(BinarySerializer<T>::deserialize(bytes, bitsOffset));
        }

        return value;
    }

    static void serialize(const std::vector<T>& value, std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        BinarySerializer<ProtocolSize>::serialize(getProtocolSizeFromStdSize(value.size()), bytes, bitsOffset);

        for (const auto& element : value)
        {
            BinarySerializer<T>::serialize(element, bytes, bitsOffset);
        }
    }
};

template<typename T>
struct BinarySerializer<Expected<T>>
{
    static Expected<T> deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        const auto success = BinarySerializer<bool>::deserialize(bytes, bitsOffset);
        if (success)
        {
            return Expected<T>::fromSuccess(BinarySerializer<T>::deserialize(bytes, bitsOffset));
        }
        else
        {
            const auto errorCode = BinarySerializer<uint32_t>::deserialize(bytes, bitsOffset);
            const auto errorMessage = BinarySerializer<std::string>::deserialize(bytes, bitsOffset);
            return Expected<T>::fromFailure(errorCode, errorMessage);
        }
    }

    static void serialize(const Expected<T>& expected, std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        BinarySerializer<bool>::serialize(expected.success(), bytes, bitsOffset);
        if (expected.success())
        {
            BinarySerializer<T>::serialize(expected.getValue(), bytes, bitsOffset);
        }
        else
        {
            BinarySerializer<uint32_t>::serialize(expected.getErrorCode(), bytes, bitsOffset);
            BinarySerializer<std::string>::serialize(expected.getErrorMessage(), bytes, bitsOffset);
        }
    }
};

}
