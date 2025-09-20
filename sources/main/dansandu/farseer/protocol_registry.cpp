#include "dansandu/farseer/protocol_registry.hpp"

using dansandu::farseer::exception::ProtocolNotRegisteredError;

namespace dansandu::farseer::protocol_registry
{

ProtocolRegistry& ProtocolRegistry::getGlobalInstance()
{
    static ProtocolRegistry protocolRegistry;
    return protocolRegistry;
}

const ProtocolRegistry::Entry& ProtocolRegistry::getProtocol(const ProtocolIdentifier identifier) const
{
    const auto lock = std::lock_guard<std::mutex>{mutex_};
    const auto position = entries_.find(identifier);
    if (position != entries_.cend())
    {
        return position->second;
    }
    THROW(ProtocolNotRegisteredError, "no protocol with identifier '", identifier, "' was registered");
}

}
