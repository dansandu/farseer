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
    u32 age;
    map<string, list<u64>> contacts;
}

message MyMessage
{
    i64 myInteger;
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
    u64 myUnsignedInteger;
    string myString;

    response
    {
        bool myBoolean;
        i64 myInteger;
    }
}
)";

        const auto protocol = parseProtocolDefinition(text);

        REQUIRE(protocol.messages.empty());

        REQUIRE(protocol.requests.size() == 1uz);

        const auto& request = protocol.requests.front();

        REQUIRE(!request.requestHasStaticSize());

        REQUIRE(request.getRequestStaticNumberOfBits().getUnderlying() == 64ul);

        REQUIRE(request.responseHasStaticSize());

        REQUIRE(request.getResponseStaticNumberOfBits().getUnderlying() == 65ul);

        REQUIRE(protocol.toString() == text);
    }

    SECTION("parsing reserved name")
    {
        const auto text = R"(namespace organization.artifact.module;

message Person
{
    string name;
    u32 static;
}
)";

        REQUIRE_THROW(ReservedNameError, parseProtocolDefinition(text));
    }
}
