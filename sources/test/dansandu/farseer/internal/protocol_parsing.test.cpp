#include "dansandu/farseer/internal/protocol_parsing.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::exception::ReservedIdentifierNameError;
using dansandu::farseer::internal::protocol_parsing::parseProtocol;

TEST_CASE("protocol_parsing")
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

        const auto protocol = parseProtocol(text);

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

        const auto protocol = parseProtocol(text);

        REQUIRE(protocol.toString() == text);
    }

    SECTION("parsing reserved identifier")
    {
        const auto text = R"(namespace organization.artifact.module;

message Person
{
    string name;
    uint32 static;
}
)";

        REQUIRE_THROW(ReservedIdentifierNameError, parseProtocol(text));
    }
}
