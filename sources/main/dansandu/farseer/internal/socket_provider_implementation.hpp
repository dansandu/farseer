#pragma once

#include "dansandu/farseer/common.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dansandu::farseer::internal::socket_provider_implementation
{

class ISocketProviderImplementation
{
public:
    ISocketProviderImplementation(const ISocketProviderImplementation& other) = delete;
    ISocketProviderImplementation(ISocketProviderImplementation&& other) noexcept = delete;
    ISocketProviderImplementation& operator=(const ISocketProviderImplementation& other) = delete;
    ISocketProviderImplementation& operator=(ISocketProviderImplementation&& other) noexcept = delete;

    ISocketProviderImplementation() = default;

    virtual ~ISocketProviderImplementation() noexcept
    {
    }

    virtual SocketIdentifier listen(const std::string& ipAddress, const int port,
                                    ConnectionCallback&& connectionCallback) = 0;

    virtual SocketIdentifier connect(const std::string& ipAddress, const int port,
                                     ConnectionCallback&& connectionCallback) = 0;

    virtual ProtocolSequenceNumber generateSequenceNumber() = 0;

    virtual void sendBytes(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) = 0;

    virtual void sendRequest(const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
                             std::vector<uint8_t>&& bytes,
                             UniqueFunction<void(std::any&&)>&& expectedResponseConsumer) = 0;

    virtual void registerMessageConsumer(const SocketIdentifier socketIdentifier,
                                         const ProtocolIdentifier protocolIdentifier,
                                         UniqueFunction<void(std::any&&)>&& messageConsumer) = 0;

    virtual void registerRequestCallback(const SocketIdentifier socketIdentifier,
                                         const ProtocolIdentifier protocolIdentifier,
                                         UniqueFunction<std::any(std::any&&)>&& requestCallback) = 0;

    virtual void close(const SocketIdentifier socketIdentifier) = 0;
};

std::shared_ptr<ISocketProviderImplementation> createSocketProviderImplementation(const bool initializeWsa);

}
