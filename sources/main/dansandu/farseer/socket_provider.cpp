#include "dansandu/farseer/socket_provider.hpp"
#include "dansandu/farseer/internal/socket_provider_implementation.hpp"

using dansandu::farseer::internal::socket_provider_implementation::createSocketProviderImplementation;
using dansandu::farseer::internal::socket_provider_implementation::ISocketProviderImplementation;

namespace dansandu::farseer::socket_provider
{

SocketProvider::SocketProvider(const bool initializeWsa)
    : implementation_{createSocketProviderImplementation(initializeWsa)}
{
}

SocketIdentifier SocketProvider::listen(
    const std::string& ipAddress, const int port,
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
) const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    return impl->listen(ipAddress, port, std::move(connectionCallback));
}

SocketIdentifier SocketProvider::connect(
    const std::string& ipAddress, const int port,
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
) const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    return impl->connect(ipAddress, port, std::move(connectionCallback));
}

ProtocolSequenceNumber SocketProvider::generateSequenceNumber() const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    return impl->generateSequenceNumber();
}

void SocketProvider::sendMessageWork(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& messageBytes) const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    impl->sendMessage(socketIdentifier, std::move(messageBytes));
}

void SocketProvider::sendRequestWork(
    const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
    std::vector<uint8_t>&& requestBytes, OneShotResponseConsumer&& responseConsumer
) const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    impl->sendRequest(socketIdentifier, sequenceNumber, std::move(requestBytes), std::move(responseConsumer));
}

void SocketProvider::registerMessageConsumerWork(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<void(std::any&&)>&& messageConsumer
) const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    impl->registerMessageConsumer(socketIdentifier, protocolIdentifier, std::move(messageConsumer));
}

void SocketProvider::registerRequestCallbackWork(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<std::any(std::any&&)>&& requestCallback
) const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    impl->registerRequestCallback(socketIdentifier, protocolIdentifier, std::move(requestCallback));
}

void SocketProvider::close(const SocketIdentifier socketIdentifier) const
{
    if (socketIdentifier != invalidSocketIdentifier)
    {
        const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

        impl->close(socketIdentifier);
    }
}

}
