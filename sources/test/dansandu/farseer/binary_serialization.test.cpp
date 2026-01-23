#include "dansandu/farseer/binary_serialization.hpp"
#include "dansandu/farseer/sample_protocol.g.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::binary_serialization::BinarySerializer;
using dansandu::farseer::protocol_metadata::ProtocolMetadata;
using dansandu::farseer::sample_protocol::DynamicMessage;
using dansandu::farseer::sample_protocol::EmptyMessage;
using dansandu::farseer::sample_protocol::StaticMessage;

TEST_CASE("binary_serialization")
{
    size_t bitsOffset = 0;

    size_t bitsCount = 0;

    SECTION("int32_t")
    {
        const auto expectedBytes = std::vector<uint8_t>({0b11111111, 0b11111111, 0b11111111, 0b11110001});

        const int32_t expected = -15;

        const auto actual = BinarySerializer<int32_t>::deserialize(expectedBytes, bitsOffset);

        REQUIRE(actual == expected);

        REQUIRE(bitsOffset == 32);

        auto actualBytes = std::vector<uint8_t>{};

        BinarySerializer<int32_t>::serialize(expected, actualBytes, bitsCount);

        REQUIRE(actualBytes == expectedBytes);

        REQUIRE(bitsCount == bitsOffset);
    }

    SECTION("int64_t")
    {
        const auto expectedBytes = std::vector<uint8_t>(
            {0b10111111, 0b11111111, 0b11111111, 0b11111111, 0b11011111, 0b11111011, 0b11111101, 0b11111110});

        const int64_t expected = -4611686018964521474ll;

        const auto actual = BinarySerializer<int64_t>::deserialize(expectedBytes, bitsOffset);

        REQUIRE(actual == expected);

        REQUIRE(bitsOffset == 64);

        auto actualBytes = std::vector<uint8_t>{};

        BinarySerializer<int64_t>::serialize(expected, actualBytes, bitsCount);

        REQUIRE(actualBytes == expectedBytes);

        REQUIRE(bitsCount == bitsOffset);
    }

    SECTION("uint32_t")
    {
        const auto expectedBytes = std::vector<uint8_t>({0b11111111, 0b11111111, 0b11111111, 0b11110001});

        const uint32_t expected = 4294967281u;

        const auto actual = BinarySerializer<uint32_t>::deserialize(expectedBytes, bitsOffset);

        REQUIRE(actual == expected);

        REQUIRE(bitsOffset == 32);

        auto actualBytes = std::vector<uint8_t>{};

        BinarySerializer<uint32_t>::serialize(expected, actualBytes, bitsCount);

        REQUIRE(actualBytes == expectedBytes);

        REQUIRE(bitsCount == bitsOffset);
    }

    SECTION("uint64_t")
    {
        const auto expectedBytes = std::vector<uint8_t>(
            {0b10111111, 0b11111111, 0b11111111, 0b11111111, 0b11011111, 0b11111011, 0b11111101, 0b11111110});

        const uint64_t expected = 13835058054745030142ull;

        const auto actual = BinarySerializer<uint64_t>::deserialize(expectedBytes, bitsOffset);

        REQUIRE(actual == expected);

        REQUIRE(bitsOffset == 64);

        auto actualBytes = std::vector<uint8_t>{};

        BinarySerializer<uint64_t>::serialize(expected, actualBytes, bitsCount);

        REQUIRE(actualBytes == expectedBytes);

        REQUIRE(bitsCount == bitsOffset);
    }

    SECTION("string")
    {
        const auto expectedBytes = std::vector<uint8_t>(
            {0b00000000, 0b00000000, 0b00000000, 0b00000100, 0b01100001, 0b01010111, 0b01111010, 0b01110101});

        const auto expected = std::string{"aWzu"};

        const auto actual = BinarySerializer<std::string>::deserialize(expectedBytes, bitsOffset);

        REQUIRE(actual == expected);

        REQUIRE(bitsOffset == 64);

        auto actualBytes = std::vector<uint8_t>{};

        BinarySerializer<std::string>::serialize(expected, actualBytes, bitsCount);

        REQUIRE(actualBytes == expectedBytes);

        REQUIRE(bitsCount == bitsOffset);
    }

    SECTION("bool")
    {
        const auto expectedBytes = std::vector<uint8_t>({0b10000000});

        const auto expected = true;

        const auto actual = BinarySerializer<bool>::deserialize(expectedBytes, bitsOffset);

        REQUIRE(actual == expected);

        REQUIRE(bitsOffset == 1);

        auto actualBytes = std::vector<uint8_t>{};

        BinarySerializer<bool>::serialize(expected, actualBytes, bitsCount);

        REQUIRE(actualBytes == expectedBytes);

        REQUIRE(bitsCount == bitsOffset);
    }

    SECTION("vector of int32_t")
    {
        const auto expectedBytes =
            std::vector<uint8_t>({0b00000000, 0b00000000, 0b00000000, 0b00000010, 0b11111111, 0b10100110, 0b01001011,
                                  0b01011100, 0b00000000, 0b00000000, 0b10011000, 0b00110111});

        const auto expected = std::vector<int32_t>({-5878948, 38967});

        const auto actual = BinarySerializer<std::vector<int32_t>>::deserialize(expectedBytes, bitsOffset);

        REQUIRE(actual == expected);

        REQUIRE(bitsOffset == 96);

        auto actualBytes = std::vector<uint8_t>{};

        BinarySerializer<std::vector<int32_t>>::serialize(expected, actualBytes, bitsCount);

        REQUIRE(actualBytes == expectedBytes);

        REQUIRE(bitsCount == bitsOffset);
    }

    SECTION("vector of vector of int32_t")
    {
        const auto expectedBytes =
            std::vector<uint8_t>({0b00000000, 0b00000000, 0b00000000, 0b00000010, 0b00000000, 0b00000000, 0b00000000,
                                  0b00000011, 0b00101111, 0b00010011, 0b11111100, 0b01101110, 0b11111111, 0b11111111,
                                  0b11111111, 0b11111110, 0b11111111, 0b11111111, 0b11110100, 0b10100101, 0b00000000,
                                  0b00000000, 0b00000000, 0b00000001, 0b11111111, 0b11110000, 0b11000000, 0b11110010});

        const auto expected = std::vector<std::vector<int32_t>>({
            {789838958, -2, -2907},
            {-999182},
        });

        const auto actual = BinarySerializer<std::vector<std::vector<int32_t>>>::deserialize(expectedBytes, bitsOffset);

        REQUIRE(actual == expected);

        REQUIRE(bitsOffset == 224);

        auto actualBytes = std::vector<uint8_t>{};

        BinarySerializer<std::vector<std::vector<int32_t>>>::serialize(expected, actualBytes, bitsCount);

        REQUIRE(actualBytes == expectedBytes);

        REQUIRE(bitsCount == bitsOffset);
    }

    auto bytes = std::vector<uint8_t>{};

    SECTION("static message")
    {
        const auto message = StaticMessage{
            .integer = 191,
            .boolean = false,
        };

        REQUIRE(ProtocolMetadata<StaticMessage>::hasStaticSize);

        REQUIRE(ProtocolMetadata<StaticMessage>::numberOfBits == 33);

        BinarySerializer<StaticMessage>::serialize(message, bytes, bitsCount);

        REQUIRE(ProtocolMetadata<StaticMessage>::numberOfBits == bitsCount);

        const auto copy = BinarySerializer<StaticMessage>::deserialize(bytes, bitsOffset);

        REQUIRE(message.integer == copy.integer);

        REQUIRE(message.boolean == copy.boolean);

        REQUIRE(ProtocolMetadata<StaticMessage>::numberOfBits == bitsOffset);
    }

    SECTION("empty message")
    {
        const auto message = EmptyMessage{};

        REQUIRE(ProtocolMetadata<EmptyMessage>::hasStaticSize);

        REQUIRE(ProtocolMetadata<EmptyMessage>::numberOfBits == 0);

        BinarySerializer<EmptyMessage>::serialize(message, bytes, bitsCount);

        BinarySerializer<EmptyMessage>::deserialize(bytes, bitsOffset);

        REQUIRE(ProtocolMetadata<EmptyMessage>::numberOfBits == bitsOffset);
    }

    SECTION("dynamic message")
    {
        const auto message = DynamicMessage{
            .messages =
                {
                    StaticMessage{
                        .integer = -982856,
                        .boolean = true,
                    },
                    StaticMessage{
                        .integer = 59920249,
                        .boolean = false,
                    },
                },
            .name = "my string",
        };

        REQUIRE(!ProtocolMetadata<DynamicMessage>::hasStaticSize);

        BinarySerializer<DynamicMessage>::serialize(message, bytes, bitsCount);

        const auto copy = BinarySerializer<DynamicMessage>::deserialize(bytes, bitsOffset);

        REQUIRE(message.messages.size() == copy.messages.size());

        REQUIRE(message.messages[0].integer == copy.messages[0].integer);

        REQUIRE(message.messages[0].boolean == copy.messages[0].boolean);

        REQUIRE(message.messages[1].integer == copy.messages[1].integer);

        REQUIRE(message.messages[1].boolean == copy.messages[1].boolean);

        REQUIRE(message.name == copy.name);
    }
}
