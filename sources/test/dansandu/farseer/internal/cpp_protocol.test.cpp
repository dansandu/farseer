#include "dansandu/farseer/internal/cpp_protocol.hpp"
#include "dansandu/ballotin/file_system.hpp"
#include "dansandu/journey/logging.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::ballotin::file_system::readAsciiFile;
using dansandu::farseer::internal::cpp_protocol::generateCppProtocol;
using dansandu::farseer::internal::protocol_definition::Field;
using dansandu::farseer::internal::protocol_definition::MessageProtocol;
using dansandu::farseer::internal::protocol_definition::ProtocolFile;
using dansandu::farseer::internal::protocol_definition::RequestProtocol;
using dansandu::farseer::internal::protocol_definition::Type;
using dansandu::farseer::internal::protocol_definition::TypeEnum;
using dansandu::journey::logging::LogWarning;

TEST_CASE("cpp_protocol")
{
    SECTION("message protocol")
    {
        const auto protocolFile = ProtocolFile{
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

        const auto cppProtocol = generateCppProtocol(protocolFile);

        LogWarning("\n", cppProtocol);
    }

    SECTION("request protocol")
    {
        const auto protocolFile = ProtocolFile{
            .fileNamespace = "org.art",
            .requests =
                {
                    RequestProtocol{
                        .identifier = "MyRequest",
                        .requestFields =
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
                        .responseFields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
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

        const auto cppProtocol = generateCppProtocol(protocolFile);

        LogWarning("\n", cppProtocol);
    }
}
