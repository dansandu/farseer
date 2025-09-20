#include "dansandu/farseer/packet_serialization.hpp"
#include "dansandu/farseer/sample_protocol.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::packet_serialization::serializeMessagePacket;
using dansandu::farseer::sample_protocol::DynamicMessage;
using dansandu::farseer::sample_protocol::EmptyMessage;
using dansandu::farseer::sample_protocol::StaticMessage;

TEST_CASE("packet_serialization")
{
    SECTION("static message")
    {
        const auto message = StaticMessage{
            .integer = 7921,
            .boolean = true,
        };

        const auto bytes = serializeMessagePacket(message);

        REQUIRE(bytes.size() == 9);

        const auto expectedBytes = std::vector<uint8_t>({0xB9, 0x91, 0xF2, 0x11, 0x00, 0x00, 0x1E, 0xF1, 0x80});

        REQUIRE(bytes == expectedBytes);
    }

    SECTION("empty message")
    {
        const auto message = EmptyMessage{};

        const auto bytes = serializeMessagePacket(message);

        REQUIRE(bytes.size() == 4);

        const auto expectedBytes = std::vector<uint8_t>({0x18, 0xCD, 0x12, 0x4D});

        REQUIRE(bytes == expectedBytes);
    }

    SECTION("dynamic message")
    {
        const auto message = DynamicMessage{
            .messages =
                {
                    StaticMessage{
                        .integer = 7365421,
                        .boolean = false,
                    },
                    StaticMessage{
                        .integer = -1650982,
                        .boolean = true,
                    },
                },
            .name = "some name",
        };

        const auto bytes = serializeMessagePacket(message);

        REQUIRE(bytes.size() == 38);

        const auto expectedBytes = std::vector<uint8_t>({
            0xE1, 0x9F, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCA, 0x00,
            0x00, 0x00, 0x02, 0x00, 0x70, 0x63, 0x2D, 0x7F, 0xF3, 0x67, 0x6D, 0x40, 0x00,
            0x00, 0x02, 0x5C, 0xDB, 0xDB, 0x59, 0x48, 0x1B, 0x98, 0x5B, 0x59, 0x40,
        });

        REQUIRE(bytes == expectedBytes);
    }
}
