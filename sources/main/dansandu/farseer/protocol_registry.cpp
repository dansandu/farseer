#include "dansandu/farseer/protocol_registry.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/exception.hpp"

using dansandu::farseer::exception::ProtocolIdentifierAlreadyRegisteredError;
using dansandu::farseer::exception::ProtocolNotRegisteredError;

namespace dansandu::farseer::protocol_registry
{

ProtocolRegistry& ProtocolRegistry::getGlobalInstance()
{
    static ProtocolRegistry protocolRegistry;
    return protocolRegistry;
}

bool ProtocolRegistry::isProtocolRegistered(const ProtocolIdentifier identifier) const
{
    const auto lock = std::lock_guard<std::mutex>{mutex_};
    return protocolDescriptors_.contains(identifier);
}

ProtocolDescriptor ProtocolRegistry::getProtocolDescriptor(const ProtocolIdentifier identifier) const
{
    const auto lock = std::lock_guard<std::mutex>{mutex_};
    const auto position = protocolDescriptors_.find(identifier);
    if (position != protocolDescriptors_.cend())
    {
        return position->second;
    }
    THROW(ProtocolNotRegisteredError, "No protocol descriptor is registered with identifier ", identifier);
}

void ProtocolRegistry::registerMessageProtocol(const ProtocolIdentifier identifier,
                                               const ProtocolDeserializer deserializer)
{
    const auto lock = std::lock_guard<std::mutex>{mutex_};
    const auto [position, inserted] =
        protocolDescriptors_.insert({identifier, ProtocolDescriptor{
                                                     .protocolType = ProtocolType::message,
                                                     .protocolDeserializer = deserializer,
                                                     .expectedResponseSerializer = nullptr,
                                                 }});
    if (!inserted)
    {
        THROW(ProtocolIdentifierAlreadyRegisteredError, "A protocol is already registered with identifier ",
              identifier);
    }
}

void ProtocolRegistry::registerRequestProtocol(const ProtocolIdentifier requestIdentifier,
                                               const ProtocolDeserializer requestDeserializer,
                                               const ProtocolIdentifier responseIdentifier,
                                               const ProtocolDeserializer expectedResponseDeserializer,
                                               const ExpectedResponseProtocolSerializer expectedResponseSerializer)
{
    const auto lock = std::lock_guard<std::mutex>{mutex_};
    const auto [requestPosition, requestInserted] =
        protocolDescriptors_.insert({requestIdentifier, ProtocolDescriptor{
                                                            .protocolType = ProtocolType::request,
                                                            .protocolDeserializer = requestDeserializer,
                                                            .expectedResponseSerializer = expectedResponseSerializer,
                                                        }});
    if (!requestInserted)
    {
        THROW(ProtocolIdentifierAlreadyRegisteredError, "A protocol is already registered with identifier ",
              requestIdentifier);
    }

    SCOPE_FAILURE([&] { protocolDescriptors_.erase(requestPosition); });

    const auto [responsePosition, responseInserted] =
        protocolDescriptors_.insert({responseIdentifier, ProtocolDescriptor{
                                                             .protocolType = ProtocolType::response,
                                                             .protocolDeserializer = expectedResponseDeserializer,
                                                             .expectedResponseSerializer = nullptr,
                                                         }});
    if (!responseInserted)
    {
        THROW(ProtocolIdentifierAlreadyRegisteredError, "A protocol is already registered with identifier ",
              responseIdentifier);
    }
}

}
