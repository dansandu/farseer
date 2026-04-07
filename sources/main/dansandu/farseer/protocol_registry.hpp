#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/protocol_serialization.hpp"

#include <any>
#include <map>
#include <mutex>

namespace dansandu::farseer::protocol_registry
{

enum class ProtocolType
{
    message,
    request,
    response,
};

struct ProtocolDescriptor
{
    ProtocolType protocolType;
    ProtocolDeserializer protocolDeserializer;
    ResponseProtocolSerializer responseSerializer;
};

class PRALINE_EXPORT ProtocolRegistry
{
public:
    static ProtocolRegistry& getGlobalInstance();

    ProtocolRegistry(const ProtocolRegistry& other) = delete;
    ProtocolRegistry(ProtocolRegistry&& other) noexcept = delete;
    ProtocolRegistry& operator=(const ProtocolRegistry& other) = delete;
    ProtocolRegistry& operator=(ProtocolRegistry&& other) noexcept = delete;

    template<typename Message>
    int registerMessageProtocol()
    {
        registerMessageProtocol(Message::Metadata::getProtocolIdentifier(),
                                dansandu::farseer::protocol_serialization::tryDeserializeMessageProtocol<Message>);
        return 0;
    }

    template<typename Request>
    int registerRequestProtocol()
    {
        registerRequestProtocol(
            Request::Metadata::getProtocolIdentifier(),
            dansandu::farseer::protocol_serialization::tryDeserializeRequestProtocol<Request>,
            Request::Response::Metadata::getProtocolIdentifier(),
            dansandu::farseer::protocol_serialization::tryDeserializeResponseProtocol<typename Request::Response>,
            dansandu::farseer::protocol_serialization::serializeResponseProtocol<typename Request::Response>);
        return 0;
    }

    bool isProtocolRegistered(const ProtocolIdentifier identifier) const;

    ProtocolDescriptor getProtocolDescriptor(const ProtocolIdentifier identifier) const;

private:
    ProtocolRegistry() = default;

    void registerMessageProtocol(const ProtocolIdentifier identifier, const ProtocolDeserializer deserializer);

    void registerRequestProtocol(const ProtocolIdentifier requestIdentifier,
                                 const ProtocolDeserializer requestDeserializer,
                                 const ProtocolIdentifier responseIdentifier,
                                 const ProtocolDeserializer responseDeserializer,
                                 const ResponseProtocolSerializer responseSerializer);

    std::map<ProtocolIdentifier, ProtocolDescriptor> protocolDescriptors_;
    mutable std::mutex mutex_;
};

}
