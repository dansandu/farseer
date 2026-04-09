#include "dansandu/farseer/internal/protocol_validation.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/protocol_definition.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::ProtocolSize;
using dansandu::farseer::exception::DuplicateFieldNameError;
using dansandu::farseer::exception::DuplicateProtocolNameError;
using dansandu::farseer::exception::MessageNameNotDefinedError;
using dansandu::farseer::exception::ProtocolFieldSelfReferenceError;
using dansandu::farseer::internal::protocol_definition::FieldDefinition;
using dansandu::farseer::internal::protocol_definition::MessageProtocolDefinition;
using dansandu::farseer::internal::protocol_definition::ProtocolDefinition;
using dansandu::farseer::internal::protocol_definition::RequestProtocolDefinition;
using dansandu::farseer::internal::protocol_definition::Type;
using dansandu::farseer::internal::protocol_definition::TypeDefinition;
using dansandu::farseer::internal::protocol_validation::validateProtocolDefinition;

TEST_CASE("protocol_validation")
{
    SECTION("message undefined message type")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .messages =
                {
                    MessageProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Person",
                        .fields =
                            {
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromMessage("House", true, ProtocolSize{32}),
                                    .name = "residence",
                                },
                            },
                    },
                },
            .requests = {},
        };

        REQUIRE_THROW(MessageNameNotDefinedError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("message defined after usage")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .messages =
                {
                    MessageProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Person",
                        .fields =
                            {
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromMessage("House", true, ProtocolSize{32}),
                                    .name = "residence",
                                },
                            },
                    },
                    MessageProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "House",
                        .fields = {},
                    },
                },
            .requests = {},
        };

        REQUIRE_THROW(MessageNameNotDefinedError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("duplicate message name")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .messages =
                {
                    MessageProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Person",
                        .fields =
                            {
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::string),
                                    .name = "name",
                                },
                            },
                    },
                    MessageProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Person",
                        .fields =
                            {
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::string),
                                    .name = "fullName",
                                },
                            },
                    },
                },
            .requests = {},
        };

        REQUIRE_THROW(DuplicateProtocolNameError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("message field self reference")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .messages =
                {
                    MessageProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Person",
                        .fields =
                            {
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::string),
                                    .name = "name",
                                },
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromMessage("Person", false, ProtocolSize{64}),
                                    .name = "parent",
                                },
                            },
                    },
                },
            .requests = {},
        };

        REQUIRE_THROW(ProtocolFieldSelfReferenceError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("message duplicate field name")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .messages =
                {
                    MessageProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Product",
                        .fields =
                            {
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::string),
                                    .name = "id",
                                },
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::u32),
                                    .name = "age",
                                },
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::i64),
                                    .name = "id",
                                },
                            },
                    },
                },
            .requests = {},
        };

        REQUIRE_THROW(DuplicateFieldNameError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("request undefined message type")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .messages = {},
            .requests =
                {
                    RequestProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Person",
                        .requestFields =
                            {
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromMessage("House", false, ProtocolSize{64}),
                                    .name = "residence",
                                },
                            },
                        .responseFields = {},
                    },
                },
        };

        REQUIRE_THROW(MessageNameNotDefinedError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("request duplicate message name")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .messages =
                {
                    MessageProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Person",
                        .fields =
                            {
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::string),
                                    .name = "name",
                                },
                            },
                    },
                },
            .requests =
                {
                    RequestProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Person",
                        .requestFields =
                            {
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::string),
                                    .name = "fullName",
                                },
                            },
                        .responseFields = {},
                    },
                },
        };

        REQUIRE_THROW(DuplicateProtocolNameError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("request field self reference")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .messages = {},
            .requests =
                {
                    RequestProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Person",
                        .requestFields =
                            {
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::string),
                                    .name = "name",
                                },
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromMessage("Person", false, ProtocolSize{64}),
                                    .name = "parent",
                                },
                            },
                        .responseFields = {},
                    },
                },
        };

        REQUIRE_THROW(ProtocolFieldSelfReferenceError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("duplicate request fields name")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .messages = {},
            .requests =
                {
                    RequestProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Product",
                        .requestFields =
                            {
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::string),
                                    .name = "id",
                                },
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::u32),
                                    .name = "age",
                                },
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::i64),
                                    .name = "id",
                                },
                            },
                        .responseFields = {},
                    },
                },
        };

        REQUIRE_THROW(DuplicateFieldNameError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("duplicate response fields name")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .messages = {},
            .requests =
                {
                    RequestProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Product",
                        .requestFields = {},
                        .responseFields =
                            {
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::string),
                                    .name = "id",
                                },
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::u32),
                                    .name = "age",
                                },
                                FieldDefinition{
                                    .typeDefinition = TypeDefinition::fromSimple(Type::i64),
                                    .name = "id",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(DuplicateFieldNameError, validateProtocolDefinition(protocolDefinition));
    }
}
