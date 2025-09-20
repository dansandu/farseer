#include "dansandu/farseer/internal/protocol_validation.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/protocol.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::exception::DuplicateFieldIdentifierError;
using dansandu::farseer::exception::DuplicateProtocolIdentifierError;
using dansandu::farseer::exception::MessageIdentifierNotDefinedError;
using dansandu::farseer::exception::ProtocolFieldSelfReferenceError;
using dansandu::farseer::internal::protocol::Field;
using dansandu::farseer::internal::protocol::MessageProtocol;
using dansandu::farseer::internal::protocol::Protocol;
using dansandu::farseer::internal::protocol::RequestProtocol;
using dansandu::farseer::internal::protocol::Type;
using dansandu::farseer::internal::protocol::TypeEnum;
using dansandu::farseer::internal::protocol_validation::validateProtocol;

TEST_CASE("protocol_validation")
{
    SECTION("message undefined message type")
    {
        const auto protocol = Protocol{
            .fileNamespace = "organization.artifact.module",
            .messages =
                {
                    MessageProtocol{
                        .identifier = "Person",
                        .fields =
                            {
                                Field{
                                    .type = Type::fromMessage("House", true, 32),
                                    .identifier = "residence",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(validateProtocol(protocol), MessageIdentifierNotDefinedError);
    }

    SECTION("message defined after usage")
    {
        const auto protocol = Protocol{
            .fileNamespace = "organization.artifact.module",
            .messages =
                {
                    MessageProtocol{
                        .identifier = "Person",
                        .fields =
                            {
                                Field{
                                    .type = Type::fromMessage("House", true, 32),
                                    .identifier = "residence",
                                },
                            },
                    },
                    MessageProtocol{
                        .identifier = "House",
                    },
                },
        };

        REQUIRE_THROW(validateProtocol(protocol), MessageIdentifierNotDefinedError);
    }

    SECTION("duplicate message identifier")
    {
        const auto protocol = Protocol{
            .fileNamespace = "organization.artifact.module",
            .messages =
                {
                    MessageProtocol{
                        .identifier = "Person",
                        .fields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
                                    .identifier = "name",
                                },
                            },
                    },
                    MessageProtocol{
                        .identifier = "Person",
                        .fields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
                                    .identifier = "fullName",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(validateProtocol(protocol), DuplicateProtocolIdentifierError);
    }

    SECTION("message field self reference")
    {
        const auto protocol = Protocol{
            .fileNamespace = "organization.artifact.module",
            .messages =
                {
                    MessageProtocol{
                        .identifier = "Person",
                        .fields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
                                    .identifier = "name",
                                },
                                Field{
                                    .type = Type::fromMessage("Person", false, 64),
                                    .identifier = "parent",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(validateProtocol(protocol), ProtocolFieldSelfReferenceError);
    }

    SECTION("message duplicate field identifier")
    {
        const auto protocol = Protocol{
            .fileNamespace = "organization.artifact.module",
            .messages =
                {
                    MessageProtocol{
                        .identifier = "Product",
                        .fields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
                                    .identifier = "id",
                                },
                                Field{
                                    .type = Type::fromSimple(TypeEnum::uint32),
                                    .identifier = "age",
                                },
                                Field{
                                    .type = Type::fromSimple(TypeEnum::int64),
                                    .identifier = "id",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(validateProtocol(protocol), DuplicateFieldIdentifierError);
    }

    SECTION("request undefined message type")
    {
        const auto protocol = Protocol{
            .fileNamespace = "organization.artifact.module",
            .requests =
                {
                    RequestProtocol{
                        .identifier = "Person",
                        .requestFields =
                            {
                                Field{
                                    .type = Type::fromMessage("House", false, 64),
                                    .identifier = "residence",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(validateProtocol(protocol), MessageIdentifierNotDefinedError);
    }

    SECTION("request duplicate message identifier")
    {
        const auto protocol = Protocol{
            .fileNamespace = "organization.artifact.module",
            .messages =
                {
                    MessageProtocol{
                        .identifier = "Person",
                        .fields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
                                    .identifier = "name",
                                },
                            },
                    },
                },
            .requests =
                {
                    RequestProtocol{
                        .identifier = "Person",
                        .requestFields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
                                    .identifier = "fullName",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(validateProtocol(protocol), DuplicateProtocolIdentifierError);
    }

    SECTION("request field self reference")
    {
        const auto protocol = Protocol{
            .fileNamespace = "organization.artifact.module",
            .requests =
                {
                    RequestProtocol{
                        .identifier = "Person",
                        .requestFields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
                                    .identifier = "name",
                                },
                                Field{
                                    .type = Type::fromMessage("Person", false, 64),
                                    .identifier = "parent",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(validateProtocol(protocol), ProtocolFieldSelfReferenceError);
    }

    SECTION("duplicate request fields identifier")
    {
        const auto protocol = Protocol{
            .fileNamespace = "organization.artifact.module",
            .requests =
                {
                    RequestProtocol{
                        .identifier = "Product",
                        .requestFields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
                                    .identifier = "id",
                                },
                                Field{
                                    .type = Type::fromSimple(TypeEnum::uint32),
                                    .identifier = "age",
                                },
                                Field{
                                    .type = Type::fromSimple(TypeEnum::int64),
                                    .identifier = "id",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(validateProtocol(protocol), DuplicateFieldIdentifierError);
    }

    SECTION("duplicate response fields identifier")
    {
        const auto protocol = Protocol{
            .fileNamespace = "organization.artifact.module",
            .requests =
                {
                    RequestProtocol{
                        .identifier = "Product",
                        .responseFields =
                            {
                                Field{
                                    .type = Type::fromSimple(TypeEnum::string),
                                    .identifier = "id",
                                },
                                Field{
                                    .type = Type::fromSimple(TypeEnum::uint32),
                                    .identifier = "age",
                                },
                                Field{
                                    .type = Type::fromSimple(TypeEnum::int64),
                                    .identifier = "id",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(validateProtocol(protocol), DuplicateFieldIdentifierError);
    }
}
