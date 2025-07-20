#include "dansandu/farseer/internal/protocol_parsing.hpp"
#include "dansandu/ballotin/file_system.hpp"
#include "dansandu/journey/logging.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::ballotin::file_system::readAsciiFile;
using dansandu::farseer::internal::protocol_definition::ProtocolFile;
using dansandu::farseer::internal::protocol_parsing::parseProtocolFile;
using dansandu::journey::logging::LogCritical;

TEST_CASE("protocol_parsing")
{
    SECTION("message parsing")
    {
        const auto text = readAsciiFile("resources/test/dansandu/farseer/message.far");

        const auto protocolFile = parseProtocolFile(text);

        REQUIRE(protocolFile.toString() == text);
    }

    SECTION("request parsing")
    {
        const auto text = readAsciiFile("resources/test/dansandu/farseer/request.far");

        const auto protocolFile = parseProtocolFile(text);

        REQUIRE(protocolFile.toString() == text);
    }
}
