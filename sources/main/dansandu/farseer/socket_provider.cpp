#include "dansandu/farseer/socket_provider.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/wsa_scope_guard.hpp"
#include "dansandu/journey/logging.hpp"

#include <string>
#include <vector>

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperationScheduler;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::wsa_scope_guard::WsaScopeGuard;

namespace dansandu::farseer::socket_provider
{

namespace
{

DWORD WINAPI consumeAsynchronousOperations(LPVOID parameter);

HANDLE createAsynchronousOperationsConsumerThread(AsynchronousOperationScheduler* const operations)
{
    auto threadId = DWORD{0};

    const auto threadAttributes = LPSECURITY_ATTRIBUTES{nullptr};
    const auto stackSize = 0;
    const auto threadFlags = DWORD{0};
    const auto threadParameter = static_cast<LPVOID>(operations);
    const auto thread = ::CreateThread(threadAttributes, stackSize, consumeAsynchronousOperations, threadParameter,
                                       threadFlags, &threadId);

    if (thread != nullptr)
    {
        return thread;
    }

    THROW(std::runtime_error, "Couldn't create asynchronous operations consumer thread: ", getLastErrorMessage());
}

struct SocketServiceProviderImplementation
{
    explicit SocketServiceProviderImplementation(const bool initializeWsa)
        : wsaScopeGuard{initializeWsa}, operations{}, thread{createAsynchronousOperationsConsumerThread(&operations)}
    {
    }

    ~SocketServiceProviderImplementation() noexcept
    {
        operations.createAbortAsynchronousOperation();

        const auto waitTimeout = INFINITE;
        ::WaitForSingleObject(thread, waitTimeout);
        ::CloseHandle(thread);
    }

    const WsaScopeGuard wsaScopeGuard;
    AsynchronousOperationScheduler operations;
    Sequencer<ProtocolSequenceNumber> sequencer;
    const HANDLE thread;
};

DWORD WINAPI consumeAsynchronousOperations(LPVOID parameter)
{
    LOG_DEBUG("Started asynchronous operations consumer thread");

    const auto operations = static_cast<AsynchronousOperationScheduler*>(parameter);

    while (operations->waitAndConsumeAsynchronousOperation())
    {
        ;
    }

    LOG_DEBUG("Exiting asynchronous operations consumer thread");

    return 0;
}

}

SocketProvider::SocketProvider(bool initializeWsa)
    : implementation_{std::make_shared<SocketServiceProviderImplementation>(initializeWsa)}
{
}

SocketProvider::~SocketProvider()
{
}

SocketIdentifier SocketProvider::listen(const std::wstring& ipAddress, const int port,
                                        ConnectionCallback connectionCallback) const
{
    const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

    return impl->operations.createListenAsynchronousOperation(ipAddress, port, std::move(connectionCallback));
}

SocketIdentifier SocketProvider::connect(const std::wstring& ipAddress, const int port,
                                         ConnectionCallback connectionCallback) const
{
    const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

    return impl->operations.createConnectAsynchronousOperation(ipAddress, port, std::move(connectionCallback));
}

ProtocolSequenceNumber SocketProvider::generateSequenceNumber() const
{
    const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

    return impl->sequencer.generate();
}

void SocketProvider::sendBytes(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) const
{
    if (socketIdentifier != invalidSocketIdentifier)
    {
        const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

        impl->operations.createSendBytesAsynchronousOperation(socketIdentifier, std::move(bytes));
    }
    else
    {
        THROW(std::logic_error, "Cannot send bytes using an invalidSocketIdentifier");
    }
}

void SocketProvider::sendRequest(const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
                                 std::vector<uint8_t>&& bytes,
                                 UniqueFunction<void(std::any&&)>&& expectedResponseConsumer) const
{
    if (socketIdentifier != invalidSocketIdentifier)
    {
        const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

        impl->operations.createSendRequestAsynchronousOperation(socketIdentifier, sequenceNumber, std::move(bytes),
                                                                std::move(expectedResponseConsumer));
    }
    else
    {
        THROW(std::logic_error, "Cannot send bytes using an invalidSocketIdentifier");
    }
}

void SocketProvider::registerMessageConsumer(const SocketIdentifier socketIdentifier,
                                             const ProtocolIdentifier protocolIdentifier,
                                             UniqueFunction<void(std::any&&)>&& messageConsumer) const
{
    if (socketIdentifier != invalidSocketIdentifier)
    {
        const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

        impl->operations.createRegisterMessageConsumerAsynchronousOperation(socketIdentifier, protocolIdentifier,
                                                                            std::move(messageConsumer));
    }
    else
    {
        THROW(std::logic_error, "Cannot register a message consumer using an invalidSocketIdentifier");
    }
}

void SocketProvider::registerRequestCallback(const SocketIdentifier socketIdentifier,
                                             const ProtocolIdentifier protocolIdentifier,
                                             UniqueFunction<std::any(std::any&&)>&& requestCallback) const
{
    if (socketIdentifier != invalidSocketIdentifier)
    {
        const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

        impl->operations.createRegisterRequestCallbackAsynchronousOperation(socketIdentifier, protocolIdentifier,
                                                                            std::move(requestCallback));
    }
    else
    {
        THROW(std::logic_error, "Cannot register a request consumer using an invalidSocketIdentifier");
    }
}

void SocketProvider::close(const SocketIdentifier socketIdentifier) const
{
    if (socketIdentifier != invalidSocketIdentifier)
    {
        const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

        impl->operations.createCloseAsynchronousOperation(socketIdentifier);
    }
}

}
