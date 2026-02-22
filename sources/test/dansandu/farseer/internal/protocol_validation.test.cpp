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
using dansandu::farseer::internal::protocol_definition::TypeDefinition;
using dansandu::farseer::internal::protocol_definition::TypeDefinitionEnum;
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
                                    .type = TypeDefinition::fromMessage("House", true, ProtocolSize{32}),
                                    .name = "residence",
                                },
                            },
                    },
                },
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
                                    .type = TypeDefinition::fromMessage("House", true, ProtocolSize{32}),
                                    .name = "residence",
                                },
                            },
                    },
                    MessageProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "House",
                    },
                },
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
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::string),
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
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::string),
                                    .name = "fullName",
                                },
                            },
                    },
                },
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
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::string),
                                    .name = "name",
                                },
                                FieldDefinition{
                                    .type = TypeDefinition::fromMessage("Person", false, ProtocolSize{64}),
                                    .name = "parent",
                                },
                            },
                    },
                },
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
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::string),
                                    .name = "id",
                                },
                                FieldDefinition{
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::uint32),
                                    .name = "age",
                                },
                                FieldDefinition{
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::int64),
                                    .name = "id",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(DuplicateFieldNameError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("request undefined message type")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .requests =
                {
                    RequestProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Person",
                        .requestFields =
                            {
                                FieldDefinition{
                                    .type = TypeDefinition::fromMessage("House", false, ProtocolSize{64}),
                                    .name = "residence",
                                },
                            },
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
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::string),
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
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::string),
                                    .name = "fullName",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(DuplicateProtocolNameError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("request field self reference")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .requests =
                {
                    RequestProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Person",
                        .requestFields =
                            {
                                FieldDefinition{
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::string),
                                    .name = "name",
                                },
                                FieldDefinition{
                                    .type = TypeDefinition::fromMessage("Person", false, ProtocolSize{64}),
                                    .name = "parent",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(ProtocolFieldSelfReferenceError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("duplicate request fields name")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .requests =
                {
                    RequestProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Product",
                        .requestFields =
                            {
                                FieldDefinition{
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::string),
                                    .name = "id",
                                },
                                FieldDefinition{
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::uint32),
                                    .name = "age",
                                },
                                FieldDefinition{
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::int64),
                                    .name = "id",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(DuplicateFieldNameError, validateProtocolDefinition(protocolDefinition));
    }

    SECTION("duplicate response fields name")
    {
        const auto protocolDefinition = ProtocolDefinition{
            .fileNamespace = "organization.artifact.module",
            .requests =
                {
                    RequestProtocolDefinition{
                        .fileNamespace = "organization.artifact.module",
                        .name = "Product",
                        .responseFields =
                            {
                                FieldDefinition{
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::string),
                                    .name = "id",
                                },
                                FieldDefinition{
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::uint32),
                                    .name = "age",
                                },
                                FieldDefinition{
                                    .type = TypeDefinition::fromSimple(TypeDefinitionEnum::int64),
                                    .name = "id",
                                },
                            },
                    },
                },
        };

        REQUIRE_THROW(DuplicateFieldNameError, validateProtocolDefinition(protocolDefinition));
    }
}
