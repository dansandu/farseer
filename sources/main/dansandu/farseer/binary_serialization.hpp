#pragma once

#include "dansandu/ballotin/binary.hpp"
#include "dansandu/ballotin/type_traits.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace dansandu::farseer::binary_serialization
{

using AssociatedUnsignedType =
    dansandu::ballotin::type_traits::TypeDictionary<dansandu::ballotin::type_traits::TypeEntry<int8_t, uint8_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<int16_t, uint16_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<int32_t, uint32_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<int64_t, uint64_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<uint8_t, uint8_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<uint16_t, uint16_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<uint32_t, uint32_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<uint64_t, uint64_t>,
                                                    dansandu::ballotin::type_traits::TypeEntry<char, unsigned char>>;

template<typename T>
struct BinarySerializer
{
    using DecayedType = std::decay_t<T>;
    using UnsignedType = AssociatedUnsignedType::Get<DecayedType>;

    static DecayedType deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        using dansandu::ballotin::binary::bitsPerByte;
        using dansandu::ballotin::binary::getMostSignificantBits;

        const auto bits = getMostSignificantBits(bytes, bitsOffset, bitsPerByte * sizeof(UnsignedType));
        bitsOffset += bitsPerByte * sizeof(UnsignedType);
        return static_cast<UnsignedType>(bits);
    }

    static void serialize(const DecayedType& value, std::vector<uint8_t>& bytes, size_t& bitsCount)
    {
        using dansandu::ballotin::binary::bitsPerByte;
        using dansandu::ballotin::binary::pushBitsMostSignificant;

        pushBitsMostSignificant(bytes, bitsCount, static_cast<UnsignedType>(value), bitsPerByte * sizeof(UnsignedType));
    }
};

template<>
struct BinarySerializer<ProtocolIdentifier>
{
    static ProtocolIdentifier deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        return ProtocolIdentifier{BinarySerializer<ProtocolIdentifier::IntegerType>::deserialize(bytes, bitsOffset)};
    }

    static void serialize(const ProtocolIdentifier value, std::vector<uint8_t>& bytes, size_t& bitsCount)
    {
        BinarySerializer<ProtocolIdentifier::IntegerType>::serialize(value.getInteger(), bytes, bitsCount);
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

    static void serialize(const bool value, std::vector<uint8_t>& bytes, size_t& bitsCount)
    {
        using dansandu::ballotin::binary::pushBitsMostSignificant;

        pushBitsMostSignificant(bytes, bitsCount, value, 1);
    }
};

template<>
struct BinarySerializer<std::string>
{
    static std::string deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        auto value = std::string{};

        const auto size = BinarySerializer<uint32_t>::deserialize(bytes, bitsOffset);

        value.reserve(size);

        for (uint32_t index = 0; index < size; ++index)
        {
            value.push_back(BinarySerializer<std::string::value_type>::deserialize(bytes, bitsOffset));
        }

        return value;
    }

    static void serialize(const std::string& value, std::vector<uint8_t>& bytes, size_t& bitsCount)
    {
        BinarySerializer<uint32_t>::serialize(static_cast<uint32_t>(value.size()), bytes, bitsCount);

        for (const auto& element : value)
        {
            BinarySerializer<std::string::value_type>::serialize(element, bytes, bitsCount);
        }
    }
};

template<typename T>
struct BinarySerializer<std::vector<T>>
{
    static std::vector<T> deserialize(const std::vector<uint8_t>& bytes, size_t& bitsOffset)
    {
        auto value = std::vector<T>{};

        const auto size = BinarySerializer<uint32_t>::deserialize(bytes, bitsOffset);

        value.reserve(size);

        for (uint32_t index = 0; index < size; ++index)
        {
            value.push_back(BinarySerializer<T>::deserialize(bytes, bitsOffset));
        }

        return value;
    }

    static void serialize(const std::vector<T>& value, std::vector<uint8_t>& bytes, size_t& bitsCount)
    {
        BinarySerializer<uint32_t>::serialize(static_cast<uint32_t>(value.size()), bytes, bitsCount);

        for (const auto& element : value)
        {
            BinarySerializer<T>::serialize(element, bytes, bitsCount);
        }
    }
};

}
