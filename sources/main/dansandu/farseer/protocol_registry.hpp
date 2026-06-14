#pragma once

#include "dansandu/farseer/common.hpp"

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
    MessageWithHeaderDeserializer messageWithHeaderDeserializer;
    SequencedProtocolWithHeaderDeserializer sequencedProtocolWithHeaderDeserializer;
    ResponseWithHeaderSerializer responseWithHeaderSerializer;
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
                                Message::Metadata::tryDeserializeWithHeader);
        return 0;
    }

    template<typename Request>
    int registerRequestProtocol()
    {
        registerRequestProtocol(Request::Metadata::getProtocolIdentifier(), Request::Metadata::tryDeserializeWithHeader,
                                Request::Response::Metadata::getProtocolIdentifier(),
                                Request::Response::Metadata::tryDeserializeWithHeader,
                                Request::Response::Metadata::serializeWithHeader);
        return 0;
    }

    bool isProtocolRegistered(const ProtocolIdentifier identifier) const;

    ProtocolDescriptor getProtocolDescriptor(const ProtocolIdentifier identifier) const;

private:
    ProtocolRegistry() = default;

    void registerMessageProtocol(const ProtocolIdentifier identifier, const MessageWithHeaderDeserializer deserializer);

    void registerRequestProtocol(const ProtocolIdentifier requestIdentifier,
                                 const SequencedProtocolWithHeaderDeserializer requestDeserializer,
                                 const ProtocolIdentifier responseIdentifier,
                                 const SequencedProtocolWithHeaderDeserializer responseDeserializer,
                                 const ResponseWithHeaderSerializer responseSerializer);

    std::map<ProtocolIdentifier, ProtocolDescriptor> protocolDescriptors_;
    mutable std::mutex mutex_;
};

}
