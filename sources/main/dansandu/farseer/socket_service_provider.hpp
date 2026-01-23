#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/packet_serialization.hpp"

#include <any>
#include <memory>
#include <string>

namespace dansandu::farseer::socket_service_provider
{

class PRALINE_EXPORT SocketServiceProvider
{
public:
    explicit SocketServiceProvider(bool initializeWsa);

    ~SocketServiceProvider();

    SocketServiceId listen(std::wstring ipAddress, const int port, ConnectionCallbackType connectionCallback) const;

    SocketServiceId connect(std::wstring ipAddress, const int port, ConnectionCallbackType connectionCallback) const;

    template<typename Message>
    void sendMessage(const SocketServiceId serviceId, const Message& message) const
    {
        using dansandu::farseer::packet_serialization::serializeMessagePacket;
        sendBytes(serviceId, serializeMessagePacket(message));
    }

    template<typename Message>
    void registerMessageConsumer(const SocketServiceId serviceId, std::function<void(Message)> messageConsumer) const
    {
        registerMessageConsumer(serviceId, Message::Metadata::getProtocolIdentifier(),
                                [messageConsumer = std::move(messageConsumer)](std::any message)
                                { messageConsumer(std::any_cast<Message>(std::move(message))); });
    }

    void close(const SocketServiceId serviceId) const;

private:
    void sendBytes(const SocketServiceId serviceId, std::vector<uint8_t> bytes) const;

    void registerMessageConsumer(const SocketServiceId serviceId, const ProtocolIdentifier protocolIdentifier,
                                 std::function<void(std::any)> messageConsumer) const;

    std::shared_ptr<void> implementation_;
};

}
