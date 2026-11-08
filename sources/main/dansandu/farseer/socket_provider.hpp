#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/exception.hpp"

#include <any>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dansandu::farseer::socket_provider
{

class PRALINE_EXPORT SocketProvider
{
public:
    explicit SocketProvider(const bool initializeWsa);

    SocketIdentifier listen(
        const std::string& ipAddress, const int port,
        UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
    ) const;

    SocketIdentifier connect(
        const std::string& ipAddress, const int port,
        UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
    ) const;

    template<typename Message>
    void sendMessage(const SocketIdentifier socketIdentifier, const Message& message) const
    {
        if (socketIdentifier == invalidSocketIdentifier)
        {
            THROW(std::logic_error, "Cannot send message using an invalidSocketIdentifier");
        }

        sendMessageWork(socketIdentifier, Message::Metadata::serializeWithHeader(message));
    }

    // The response consumer is guaranteed to be invoked. If the consumer doesn't reach the socket provider internal
    // thread, then the response consumer will be invoked on the caller thread. Future consideration: add a dedicated
    // thread to invoke callbacks for consistency, or switch to std::future<T> instead of callbacks.
    template<typename Request>
    void sendRequest(
        const SocketIdentifier socketIdentifier, const Request& request,
        const std::optional<std::chrono::milliseconds> timeout,
        UniqueFunction<void(Expected<typename Request::Response>&&)>&& responseConsumer
    ) const
    {
        const auto deadline = timeout.transform(getDeadlineFromTimeout);

        auto oneShotResponseConsumer = OneShotResponseConsumer{
            Request::Response::Metadata::getProtocolIdentifier(), Request::Response::Metadata::getFailedResponse,
            deadline, [responseConsumer = std::move(responseConsumer)](std::any&& response)
            { responseConsumer(std::any_cast<Expected<typename Request::Response>&&>(std::move(response))); }
        };

        if (socketIdentifier == invalidSocketIdentifier)
        {
            THROW(std::logic_error, "Cannot send request using an invalidSocketIdentifier");
        }

        const auto sequenceNumber = generateSequenceNumber();

        sendRequestWork(
            socketIdentifier, sequenceNumber, Request::Metadata::serializeWithHeader(request, sequenceNumber),
            std::move(oneShotResponseConsumer)
        );
    }

    template<typename Message>
    void registerMessageConsumer(
        const SocketIdentifier socketIdentifier, UniqueFunction<void(Message&&)>&& messageConsumer
    ) const
    {
        if (socketIdentifier == invalidSocketIdentifier)
        {
            THROW(std::logic_error, "Cannot register message consumer using an invalidSocketIdentifier");
        }

        registerMessageConsumerWork(
            socketIdentifier, Message::Metadata::getProtocolIdentifier(),
            [messageConsumer = std::move(messageConsumer)](std::any&& message)
            { messageConsumer(std::any_cast<Message&&>(std::move(message))); }
        );
    }

    template<typename Request>
    void registerRequestCallback(
        const SocketIdentifier socketIdentifier, UniqueFunction<typename Request::Response(Request&&)>&& requestCallback
    ) const
    {
        if (socketIdentifier == invalidSocketIdentifier)
        {
            THROW(std::logic_error, "Cannot register request callback using an invalidSocketIdentifier");
        }

        registerRequestCallbackWork(
            socketIdentifier, Request::Metadata::getProtocolIdentifier(),
            [requestCallback = std::move(requestCallback)](std::any&& request)
            { return Request::Response::Metadata::invokeRequestCallback(requestCallback, std::move(request)); }
        );
    }

    void close(const SocketIdentifier socketIdentifier) const;

private:
    ProtocolSequenceNumber generateSequenceNumber() const;

    void sendMessageWork(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& messageBytes) const;

    void sendRequestWork(
        const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
        std::vector<uint8_t>&& requestBytes, OneShotResponseConsumer&& responseConsumer
    ) const;

    void registerMessageConsumerWork(
        const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
        UniqueFunction<void(std::any&&)>&& messageConsumer
    ) const;

    void registerRequestCallbackWork(
        const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
        UniqueFunction<std::any(std::any&&)>&& requestCallback
    ) const;

    std::shared_ptr<void> implementation_;
};

}
