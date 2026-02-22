#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/protocol_definition_parsing.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::exception::ReservedNameError;
using dansandu::farseer::internal::protocol_definition_parsing::parseProtocolDefinition;

TEST_CASE("protocol_definition_parsing")
{
    SECTION("message parsing")
    {
        const auto text = R"(namespace organization.artifact.module;

message Person
{
    string name;
    uint32 age;
}

message MyMessage
{
    int64 myInteger;
    Person parent;
    list<Person> children;
}
)";

        const auto protocol = parseProtocolDefinition(text);

        REQUIRE(protocol.toString() == text);
    }

    SECTION("request parsing")
    {
        const auto text = R"(namespace organization.artifact;

request MyRequest
{
    uint64 myUnsignedInteger;
    string myString;

    response
    {
        bool myBoolean;
        int64 myInteger;
    }
}
)";

        const auto protocol = parseProtocolDefinition(text);

        REQUIRE(protocol.messages.empty());

        REQUIRE(protocol.requests.size() == 1ULL);

        const auto& request = protocol.requests.front();

        REQUIRE(!request.requestHasStaticSize);

        REQUIRE(request.requestStaticNumberOfBits.getUnderlying() == 64UL);

        REQUIRE(request.responseHasStaticSize);

        REQUIRE(request.responseStaticNumberOfBits.getUnderlying() == 65UL);

        REQUIRE(protocol.toString() == text);
    }

    SECTION("parsing reserved name")
    {
        const auto text = R"(namespace organization.artifact.module;

message Person
{
    string name;
    uint32 static;
}
)";

        REQUIRE_THROW(ReservedNameError, parseProtocolDefinition(text));
    }
}
