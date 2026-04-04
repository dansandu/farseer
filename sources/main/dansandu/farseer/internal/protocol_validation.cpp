#include "dansandu/farseer/internal/protocol_validation.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/protocol_definition.hpp"

#include <set>
#include <string>
#include <string_view>

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

namespace dansandu::farseer::internal::protocol_validation
{

namespace
{

void validateFieldDefinitions(const std::vector<FieldDefinition>& fields, const std::set<std::string>& messageNames,
                              const std::string_view protocolName)
{
    auto fieldNames = std::set<std::string>{};

    for (const auto& field : fields)
    {
        auto queue = std::vector<const TypeDefinition*>{{&field.type}};

        for (auto index = size_t{}; index < queue.size(); ++index)
        {
            const auto type = queue[index];

            if (type->getTypeEnum() == TypeDefinitionEnum::message)
            {
                if (protocolName == type->getName())
                {
                    THROW(ProtocolFieldSelfReferenceError, "protocol ", protocolName, " field cannot reference itself");
                }

                if (!messageNames.contains(type->getName()))
                {
                    THROW(MessageNameNotDefinedError, "the name ", type->getName(), " was not defined");
                }
            }

            for (const auto& subType : type->getSubtypes())
            {
                queue.push_back(&subType);
            }
        }

        if (fieldNames.contains(field.name))
        {
            THROW(DuplicateFieldNameError, "the field name ", field.name, " is already used by another field");
        }

        fieldNames.insert(field.name);
    }
}

void validateMessageDefinitions(const std::vector<MessageProtocolDefinition>& messages,
                                std::set<std::string>& messageNames)
{
    for (const auto& message : messages)
    {
        if (messageNames.contains(message.name))
        {
            THROW(DuplicateProtocolNameError, "the protocol name ", message.name,
                  " is already used by another protocol");
        }

        messageNames.insert(message.name);

        validateFieldDefinitions(message.fields, messageNames, message.name);
    }
}

void validateRequestDefinitions(const std::vector<RequestProtocolDefinition>& requests,
                                const std::set<std::string>& messageNames)
{
    auto requestNames = std::set<std::string>{};

    for (const auto& request : requests)
    {
        if (messageNames.contains(request.name) || requestNames.contains(request.name))
        {
            THROW(DuplicateProtocolNameError, "the protocol name ", request.name,
                  " is already used by another protocol");
        }

        requestNames.insert(request.name);

        validateFieldDefinitions(request.requestFields, messageNames, request.name);

        validateFieldDefinitions(request.responseFields, messageNames, request.name);
    }
}

}

void validateProtocolDefinition(const ProtocolDefinition& protocol)
{
    auto messageNames = std::set<std::string>{};

    validateMessageDefinitions(protocol.messages, messageNames);

    validateRequestDefinitions(protocol.requests, messageNames);
}

}
