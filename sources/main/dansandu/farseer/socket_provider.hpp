#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/protocol_serialization.hpp"

#include <any>
#include <memory>
#include <string>
#include <vector>

namespace dansandu::farseer::socket_provider
{

class PRALINE_EXPORT SocketProvider
{
public:
    explicit SocketProvider(const bool initializeWsa);

    SocketIdentifier listen(const std::wstring& ipAddress, const int port, ConnectionCallback connectionCallback) const;

    SocketIdentifier connect(const std::wstring& ipAddress, const int port,
                             ConnectionCallback connectionCallback) const;

    template<typename Message>
    void sendMessage(const SocketIdentifier socketIdentifier, const Message& message) const
    {
        if (socketIdentifier == invalidSocketIdentifier)
        {
            THROW(std::logic_error, "Cannot send message using an invalidSocketIdentifier");
        }

        using dansandu::farseer::protocol_serialization::serializeMessageProtocol;
        sendBytes(socketIdentifier, serializeMessageProtocol(message));
    }

    template<typename Request>
    void sendRequest(const SocketIdentifier socketIdentifier, const Request& request,
                     UniqueFunction<void(Expected<typename Request::Response>&&)> expectedResponseConsumer) const
    {
        if (socketIdentifier == invalidSocketIdentifier)
        {
            THROW(std::logic_error, "Cannot send request using an invalidSocketIdentifier");
        }

        using dansandu::farseer::protocol_serialization::serializeRequestProtocol;
        const auto sequenceNumber = generateSequenceNumber();
        sendRequest(socketIdentifier, sequenceNumber, serializeRequestProtocol(request, sequenceNumber),
                    [expectedResponseConsumer = std::move(expectedResponseConsumer)](std::any&& expectedResponse)
                    {
                        expectedResponseConsumer(
                            std::any_cast<Expected<typename Request::Response>&&>(std::move(expectedResponse)));
                    });
    }

    template<typename Message>
    void registerMessageConsumer(const SocketIdentifier socketIdentifier,
                                 UniqueFunction<void(Message&&)> messageConsumer) const
    {
        if (socketIdentifier == invalidSocketIdentifier)
        {
            THROW(std::logic_error, "Cannot register message consumer using an invalidSocketIdentifier");
        }

        registerMessageConsumer(socketIdentifier, Message::Metadata::getProtocolIdentifier(),
                                [messageConsumer = std::move(messageConsumer)](std::any&& message)
                                { messageConsumer(std::any_cast<Message&&>(std::move(message))); });
    }

    template<typename Request>
    void registerRequestCallback(const SocketIdentifier socketIdentifier,
                                 UniqueFunction<typename Request::Response(Request&&)> requestCallback) const
    {
        if (socketIdentifier == invalidSocketIdentifier)
        {
            THROW(std::logic_error, "Cannot register request callback using an invalidSocketIdentifier");
        }

        registerRequestCallback(
            socketIdentifier, Request::Metadata::getProtocolIdentifier(),
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

    void close(const SocketIdentifier socketIdentifier) const;

private:
    ProtocolSequenceNumber generateSequenceNumber() const;

    void sendBytes(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) const;

    void sendRequest(const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
                     std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& expectedResponseConsumer) const;

    void registerMessageConsumer(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                 UniqueFunction<void(std::any&&)>&& messageConsumer) const;

    void registerRequestCallback(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                 UniqueFunction<std::any(std::any&&)>&& requestCallback) const;

    std::shared_ptr<void> implementation_;
};

}
