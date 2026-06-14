#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/exception.hpp"

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

    SocketIdentifier listen(const std::string& ipAddress, const int port,
                            UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback) const;

    SocketIdentifier
    connect(const std::string& ipAddress, const int port,
            UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback) const;

    template<typename Message>
    void sendMessage(const SocketIdentifier socketIdentifier, const Message& message) const
    {
        if (socketIdentifier == invalidSocketIdentifier)
        {
            THROW(std::logic_error, "Cannot send message using an invalidSocketIdentifier");
        }

        sendBytes(socketIdentifier, Message::Metadata::serializeWithHeader(message));
    }

    template<typename Request>
    void sendRequest(const SocketIdentifier socketIdentifier, const Request& request,
                     UniqueFunction<void(Expected<typename Request::Response>&&)>&& responseConsumer) const
    {
        if (socketIdentifier == invalidSocketIdentifier)
        {
            THROW(std::logic_error, "Cannot send request using an invalidSocketIdentifier");
        }

        const auto sequenceNumber = generateSequenceNumber();
        sendRequest(socketIdentifier, sequenceNumber, Request::Metadata::serializeWithHeader(request, sequenceNumber),
                    [responseConsumer = std::move(responseConsumer)](std::any&& response)
                    { responseConsumer(std::any_cast<Expected<typename Request::Response>&&>(std::move(response))); });
    }

    template<typename Message>
    void registerMessageConsumer(const SocketIdentifier socketIdentifier,
                                 UniqueFunction<void(Message&&)>&& messageConsumer) const
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
                                 UniqueFunction<typename Request::Response(Request&&)>&& requestCallback) const
    {
        if (socketIdentifier == invalidSocketIdentifier)
        {
            THROW(std::logic_error, "Cannot register request callback using an invalidSocketIdentifier");
        }

        using Response = typename Request::Response;

        registerRequestCallback(socketIdentifier, Request::Metadata::getProtocolIdentifier(),
                                [requestCallback = std::move(requestCallback)](std::any&& request) -> std::any
                                {
                                    try
                                    {
                                        return Expected<Response>::fromSuccess(
                                            requestCallback(std::any_cast<Request&&>(std::move(request))));
                                    }
                                    catch (const RequestProtocolError& exception)
                                    {
                                        return Expected<Response>::fromFailure(exception.getErrorCode(),
                                                                               exception.getErrorMessage());
                                    }
                                    catch (const std::exception& exception)
                                    {
                                        return Expected<Response>::fromInternalServerError(exception.what());
                                    }
                                    catch (...)
                                    {
                                        return Expected<Response>::fromInternalServerError();
                                    }
                                });
    }

    void close(const SocketIdentifier socketIdentifier) const;

private:
    ProtocolSequenceNumber generateSequenceNumber() const;

    void sendBytes(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) const;

    void sendRequest(const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
                     std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer) const;

    void registerMessageConsumer(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                 UniqueFunction<void(std::any&&)>&& messageConsumer) const;

    void registerRequestCallback(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                 UniqueFunction<std::any(std::any&&)>&& requestCallback) const;

    std::shared_ptr<void> implementation_;
};

}
