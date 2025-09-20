#include "dansandu/farseer/internal/protocol_validation.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/protocol.hpp"

#include <set>
#include <string>
#include <string_view>

using dansandu::farseer::exception::DuplicateFieldIdentifierError;
using dansandu::farseer::exception::DuplicateProtocolIdentifierError;
using dansandu::farseer::exception::MessageIdentifierNotDefinedError;
using dansandu::farseer::exception::ProtocolFieldSelfReferenceError;
using dansandu::farseer::internal::protocol::Field;
using dansandu::farseer::internal::protocol::MessageProtocol;
using dansandu::farseer::internal::protocol::Protocol;
using dansandu::farseer::internal::protocol::RequestProtocol;
using dansandu::farseer::internal::protocol::TypeEnum;

namespace dansandu::farseer::internal::protocol_validation
{

namespace
{

void validateFields(const std::vector<Field>& fields, const std::set<std::string>& messageIdentifiers,
                    const std::string_view protocolIdentifier)
{
    auto fieldIdentifiers = std::set<std::string>{};

    for (const auto& field : fields)
    {
        auto typePointer = &field.type;

        while (typePointer != nullptr)
        {
            if (typePointer->getTypeEnum() == TypeEnum::message)
            {
                if (protocolIdentifier == typePointer->getIdentifier())
                {
                    THROW(ProtocolFieldSelfReferenceError, "protocol ", protocolIdentifier,
                          " field cannot reference itself");
                }

                if (!messageIdentifiers.contains(typePointer->getIdentifier()))
                {
                    THROW(MessageIdentifierNotDefinedError, "the identifier ", typePointer->getIdentifier(),
                          " was not defined");
                }
            }

            typePointer = typePointer->getSubtype();
        }

        if (fieldIdentifiers.contains(field.identifier))
        {
            THROW(DuplicateFieldIdentifierError, "the field identifier ", field.identifier,
                  " is already used by another field");
        }

        fieldIdentifiers.insert(field.identifier);
    }
}

void validateMessages(const std::vector<MessageProtocol>& messages, std::set<std::string>& messageIdentifiers)
{
    for (const auto& message : messages)
    {
        if (messageIdentifiers.contains(message.identifier))
        {
            THROW(DuplicateProtocolIdentifierError, "the protocol identifier ", message.identifier,
                  " is already used by another protocol");
        }

        messageIdentifiers.insert(message.identifier);

        validateFields(message.fields, messageIdentifiers, message.identifier);
    }
}

void validateRequests(const std::vector<RequestProtocol>& requests, const std::set<std::string>& messageIdentifiers)
{
    auto requestIdentifiers = std::set<std::string>{};

    for (const auto& request : requests)
    {
        if (messageIdentifiers.contains(request.identifier) || requestIdentifiers.contains(request.identifier))
        {
            THROW(DuplicateProtocolIdentifierError, "the protocol identifier ", request.identifier,
                  " is already used by another protocol");
        }

        requestIdentifiers.insert(request.identifier);

        validateFields(request.requestFields, messageIdentifiers, request.identifier);

        validateFields(request.responseFields, messageIdentifiers, request.identifier);
    }
}

}

void validateProtocol(const Protocol& protocol)
{
    auto messageIdentifiers = std::set<std::string>{};

    validateMessages(protocol.messages, messageIdentifiers);

    validateRequests(protocol.requests, messageIdentifiers);
}

}
