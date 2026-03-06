#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/protocol_serialization.hpp"

#include <any>
#include <memory>
#include <string>
#include <vector>

namespace dansandu::farseer::socket_service_provider
{

class PRALINE_EXPORT SocketServiceProvider
{
public:
    explicit SocketServiceProvider(bool initializeWsa);

    ~SocketServiceProvider();

    SocketServiceId listen(const std::wstring& ipAddress, const int port,
                           ConnectionCallbackType connectionCallback) const;

    SocketServiceId connect(const std::wstring& ipAddress, const int port,
                            ConnectionCallbackType connectionCallback) const;

    template<typename Message>
    void sendMessage(const SocketServiceId serviceId, const Message& message) const
    {
        using dansandu::farseer::protocol_serialization::serializeMessageProtocol;
        sendBytes(serviceId, serializeMessageProtocol(message));
    }

    template<typename Request>
    void sendRequest(const SocketServiceId serviceId, const Request& request,
                     UniqueFunction<void(Expected<typename Request::Response>&&)> expectedResponseConsumer) const
    {
        using dansandu::farseer::protocol_serialization::serializeRequestProtocol;
        const auto sequenceNumber = generateSequenceNumber();
        sendRequest(serviceId, sequenceNumber, serializeRequestProtocol(request, sequenceNumber),
                    [expectedResponseConsumer = std::move(expectedResponseConsumer)](std::any&& expectedResponse)
                    {
                        expectedResponseConsumer(
                            std::any_cast<Expected<typename Request::Response>&&>(std::move(expectedResponse)));
                    });
    }

    template<typename Message>
    void registerMessageConsumer(const SocketServiceId serviceId, UniqueFunction<void(Message&&)> messageConsumer) const
    {
        registerMessageConsumer(serviceId, Message::Metadata::getProtocolIdentifier(),
                                [messageConsumer = std::move(messageConsumer)](std::any&& message)
                                { messageConsumer(std::any_cast<Message&&>(std::move(message))); });
    }

    template<typename Request>
    void registerRequestCallback(const SocketServiceId serviceId,
                                 UniqueFunction<typename Request::Response(Request&&)> requestCallback) const
    {
        registerRequestCallback(
            serviceId, Request::Metadata::getProtocolIdentifier(),
            [requestCallback = std::move(requestCallback)](std::any&& request) -> std::any
            {
                try
                {
                    return Expected<typename Request::Response>::fromSuccess(
                        requestCallback(std::any_cast<Request&&>(std::move(request))));
                }
                catch (const RequestProtocolError& exception)
                {
                    return Expected<typename Request::Response>::fromFailure(exception.getErrorCode(),
                                                                             exception.getErrorMessage());
                }
                catch (const std::exception& exception)
                {
                    return Expected<typename Request::Response>::fromInternalServerError(exception.what());
                }
                catch (...)
                {
                    return Expected<typename Request::Response>::fromInternalServerError();
                }
            });
    }

    void close(const SocketServiceId serviceId) const;

private:
    ProtocolSequenceNumber generateSequenceNumber() const;

    void sendBytes(const SocketServiceId serviceId, std::vector<uint8_t>&& bytes) const;

    void sendRequest(const SocketServiceId serviceId, const ProtocolSequenceNumber sequenceNumber,
                     std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& expectedResponseConsumer) const;

    void registerMessageConsumer(const SocketServiceId serviceId, const ProtocolIdentifier protocolIdentifier,
                                 UniqueFunction<void(std::any&&)>&& messageConsumer) const;

    void registerRequestCallback(const SocketServiceId serviceId, const ProtocolIdentifier protocolIdentifier,
                                 UniqueFunction<std::any(std::any&&)>&& requestCallback) const;

    std::shared_ptr<void> implementation_;
};

}
