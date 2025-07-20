#include "dansandu/farseer/internal/cpp_protocol.hpp"
#include "dansandu/ballotin/file_system.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::ballotin::file_system::readAsciiFile;
using dansandu::farseer::internal::cpp_protocol::generateCppProtocol;
using dansandu::farseer::internal::protocol::Field;
using dansandu::farseer::internal::protocol::MessageProtocol;
using dansandu::farseer::internal::protocol::Protocol;
using dansandu::farseer::internal::protocol::RequestProtocol;
using dansandu::farseer::internal::protocol::Type;
using dansandu::farseer::internal::protocol::TypeEnum;

TEST_CASE("cpp_protocol")
{
    SECTION("message protocol")
    {
        const auto protocol = Protocol{
            .fileNamespace = "org.art",
            .messages =
                {
                    MessageProtocol{
                        .identifier = "MyMessage",
                        .fields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::int64),
                                    .identifier = "number",
                                },
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
                                    .identifier = "myString",
                                },
                            },
                    },
                },
        };

        const auto expected = R"(#include <cstdint>
#include <string>
#include <vector>

namespace org::art
{

struct MyMessage
{
    int64_t number;
    std::string myString;
};

}
)";

        const auto cppProtocol = generateCppProtocol(protocol);

        REQUIRE(cppProtocol == expected);
    }

    SECTION("request protocol")
    {
        const auto protocol = Protocol{
            .fileNamespace = "someorg.someart.module.folder",
            .requests =
                {
                    RequestProtocol{
                        .identifier = "MyRequest",
                        .requestFields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
                                    .identifier = "password",
                                },
                                Field{
                                    .type = Type::fromSimple(TypeEnum::boolean),
                                    .identifier = "canExecute",
                                },
                            },
                        .responseFields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
                                    .identifier = "hash",
                                },
                                Field{
                                    .type = Type::fromList(Type::fromList(Type::fromSimple(TypeEnum::string))),
                                    .identifier = "stringTable",
                                },
                            },
                    },
                },
        };

        const auto expected = R"(#include <cstdint>
#include <string>
#include <vector>

namespace someorg::someart::module::folder
{

struct MyRequest
{
    std::string password;
    bool canExecute;

    struct response
    {
        std::string hash;
        std::vector<std::vector<std::string>> stringTable;
    };
};

}
)";

        const auto cppProtocol = generateCppProtocol(protocol);

        REQUIRE(cppProtocol == expected);
    }
}
