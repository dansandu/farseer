#include "dansandu/farseer/protocol_serialization.hpp"
#include "dansandu/farseer/sample_protocol.g.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::Expected;
using dansandu::farseer::ProtocolSequenceNumber;
using dansandu::farseer::protocol_serialization::serializeExpectedResponseProtocol;
using dansandu::farseer::protocol_serialization::serializeMessageProtocol;
using dansandu::farseer::protocol_serialization::serializeRequestProtocol;
using dansandu::farseer::sample_protocol::DynamicMessage;
using dansandu::farseer::sample_protocol::EmptyMessage;
using dansandu::farseer::sample_protocol::MyRequest;
using dansandu::farseer::sample_protocol::StaticMessage;

TEST_CASE("protocol_serialization")
{
    SECTION("static message")
    {
        const auto message = StaticMessage{
            .integer = 0x1EF1,
            .boolean = true,
        };

        const auto bytes = serializeMessageProtocol(message);

        REQUIRE(bytes.size() == 9);

        const auto expectedBytes = std::vector<uint8_t>({0xB9, 0x91, 0xF2, 0x11, 0x00, 0x00, 0x1E, 0xF1, 0x80});

        REQUIRE(bytes == expectedBytes);
    }

    SECTION("empty message")
    {
        const auto message = EmptyMessage{};

        const auto bytes = serializeMessageProtocol(message);

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
                        .integer = 0x70632D,
                        .boolean = false,
                    },
                    StaticMessage{
                        .integer = -1650982,
                        .boolean = true,
                    },
                },
            .name = "some name",
        };

        const auto bytes = serializeMessageProtocol(message);

        REQUIRE(bytes.size() == 34);

        const auto expectedBytes = std::vector<uint8_t>({
            0xE1, 0x9F, 0x01, 0x03, 0x00, 0x00, 0x00, 0xCA, 0x00, 0x00, 0x00, 0x02, 0x00, 0x70, 0x63, 0x2D, 0x7F,
            0xF3, 0x67, 0x6D, 0x40, 0x00, 0x00, 0x02, 0x5C, 0xDB, 0xDB, 0x59, 0x48, 0x1B, 0x98, 0x5B, 0x59, 0x40,
        });

        REQUIRE(bytes == expectedBytes);
    }

    SECTION("request")
    {
        const auto request = MyRequest{
            .user = "jimmy",
            .password = "123456",
        };

        const auto sequenceNumber = ProtocolSequenceNumber{0x2CE};

        const auto bytes = serializeRequestProtocol(request, sequenceNumber);

        REQUIRE(bytes.size() == 35);

        const auto expectedBytes = std::vector<uint8_t>(
            {0x08, 0x5A, 0x55, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xCE, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00,
             0x00, 0x05, 0x6A, 0x69, 0x6D, 0x6D, 0x79, 0x00, 0x00, 0x00, 0x06, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36});

        REQUIRE(bytes == expectedBytes);
    }

    SECTION("response")
    {
        using Response = typename MyRequest::Response;

        SECTION("success")
        {
            const auto response = Expected<Response>::fromSuccess(Response{
                .contacts = {"mars", "pluto", "saturn"},
                .authenticationToken = 0xB1067FACU,
            });

            const auto sequenceNumber = ProtocolSequenceNumber{0xF15D};

            const auto bytes = serializeExpectedResponseProtocol<Response>(response, sequenceNumber);

            REQUIRE(bytes.size() == 56);

            const auto expectedBytes = std::vector<uint8_t>(
                {0x08, 0x10, 0x31, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF1, 0x5D, 0x00, 0x00,
                 0x01, 0x39, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x02, 0x36, 0xB0, 0xB9, 0x39,
                 0x80, 0x00, 0x00, 0x02, 0xB8, 0x36, 0x3A, 0xBA, 0x37, 0x80, 0x00, 0x00, 0x03, 0x39,
                 0xB0, 0xBA, 0x3A, 0xB9, 0x37, 0x00, 0x00, 0x00, 0x00, 0x58, 0x83, 0x3F, 0xD6, 0x00});

            REQUIRE(bytes == expectedBytes);
        }

        SECTION("failure")
        {
            const auto errorCode = 0x74F3;

            const auto errorMessage = "Server error";

            const auto response = Expected<Response>::fromFailure(errorCode, errorMessage);

            const auto sequenceNumber = ProtocolSequenceNumber{0x29B};

            const auto bytes = serializeExpectedResponseProtocol<Response>(response, sequenceNumber);

            REQUIRE(bytes.size() == 37);

            const auto expectedBytes =
                std::vector<uint8_t>({0x08, 0x10, 0x31, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x9B, 0x00,
                                      0x00, 0x00, 0xA1, 0x00, 0x00, 0x3A, 0x79, 0x80, 0x00, 0x00, 0x06, 0x29, 0xB2,
                                      0xB9, 0x3B, 0x32, 0xB9, 0x10, 0x32, 0xB9, 0x39, 0x37, 0xB9, 0x00});

            REQUIRE(bytes == expectedBytes);
        }
    }
}
