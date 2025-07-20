#include "dansandu/farseer/internal/protocol_parsing.hpp"
#include "dansandu/ballotin/file_system.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::ballotin::file_system::readAsciiFile;
using dansandu::farseer::internal::protocol::Protocol;
using dansandu::farseer::internal::protocol_parsing::parseProtocol;

TEST_CASE("protocol_parsing")
{
    SECTION("message parsing")
    {
        const auto text = readAsciiFile("resources/test/dansandu/farseer/message.far");

        const auto protocol = parseProtocol(text);

        REQUIRE(protocol.toString() == text);
    }

    SECTION("request parsing")
    {
        const auto text = readAsciiFile("resources/test/dansandu/farseer/request.far");

        const auto protocol = parseProtocol(text);

        REQUIRE(protocol.toString() == text);
    }
}
