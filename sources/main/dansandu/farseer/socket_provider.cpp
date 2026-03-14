#include "dansandu/farseer/socket_provider.hpp"
#include "dansandu/farseer/internal/socket_provider_implementation.hpp"
#include "dansandu/farseer/internal/windows/windows_socket_provider_implementation.hpp"

using dansandu::farseer::internal::socket_provider_implementation::ISocketProviderImplementation;
using dansandu::farseer::internal::windows::windows_socket_provider_implementation::
    createWindowsSocketProviderImplementation;

namespace dansandu::farseer::socket_provider
{

SocketProvider::SocketProvider(const bool initializeWsa)
    : implementation_{createWindowsSocketProviderImplementation(initializeWsa)}
{
}

SocketIdentifier SocketProvider::listen(const std::wstring& ipAddress, const int port,
                                        ConnectionCallback connectionCallback) const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    return impl->listen(ipAddress, port, std::move(connectionCallback));
}

SocketIdentifier SocketProvider::connect(const std::wstring& ipAddress, const int port,
                                         ConnectionCallback connectionCallback) const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    return impl->connect(ipAddress, port, std::move(connectionCallback));
}

ProtocolSequenceNumber SocketProvider::generateSequenceNumber() const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    return impl->generateSequenceNumber();
}

void SocketProvider::sendBytes(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    impl->sendBytes(socketIdentifier, std::move(bytes));
}

void SocketProvider::sendRequest(const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
                                 std::vector<uint8_t>&& bytes,
                                 UniqueFunction<void(std::any&&)>&& expectedResponseConsumer) const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    impl->sendRequest(socketIdentifier, sequenceNumber, std::move(bytes), std::move(expectedResponseConsumer));
}

void SocketProvider::registerMessageConsumer(const SocketIdentifier socketIdentifier,
                                             const ProtocolIdentifier protocolIdentifier,
                                             UniqueFunction<void(std::any&&)>&& messageConsumer) const
{
    const auto impl = static_cast<ISocketProviderImplementation*>(implementation_.get());

    impl->registerMessageConsumer(socketIdentifier, protocolIdentifier, std::move(messageConsumer));
}

void SocketProvider::registerRequestCallback(const SocketIdentifier socketIdentifier,
                                             const ProtocolIdentifier protocolIdentifier,
                                             UniqueFunction<std::any(std::any&&)>&& requestCallback) const
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
