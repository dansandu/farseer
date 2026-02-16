#include "dansandu/farseer/socket_service_provider.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/socket_service.hpp"
#include "dansandu/farseer/internal/windows/wsa_scope_guard.hpp"
#include "dansandu/journey/logging.hpp"

#include <string>
#include <vector>

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperationContainer;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::wsa_scope_guard::WsaScopeGuard;

namespace dansandu::farseer::socket_service_provider
{

namespace
{

DWORD WINAPI consumeAsynchronousOperations(LPVOID parameter);

HANDLE createAsynchronousOperationsConsumerThread(AsynchronousOperationContainer* const operations)
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
    AsynchronousOperationContainer operations;
    Sequencer<ProtocolSequenceNumber> sequencer;
    const HANDLE thread;
};

DWORD WINAPI consumeAsynchronousOperations(LPVOID parameter)
{
    LOG_DEBUG("Started asynchronous operations consumer thread");

    const auto operations = static_cast<AsynchronousOperationContainer*>(parameter);

    while (operations->waitAndConsumeAsynchronousOperation())
    {
        ;
    }

    LOG_DEBUG("Exiting asynchronous operations consumer thread");

    return 0;
}

}

SocketServiceProvider::SocketServiceProvider(bool initializeWsa)
    : implementation_{std::make_shared<SocketServiceProviderImplementation>(initializeWsa)}
{
}

SocketServiceProvider::~SocketServiceProvider()
{
}

SocketServiceId SocketServiceProvider::listen(const std::wstring& ipAddress, const int port,
                                              ConnectionCallbackType connectionCallback) const
{
    const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

    return impl->operations.createListenAsynchronousOperation(ipAddress, port, std::move(connectionCallback));
}

SocketServiceId SocketServiceProvider::connect(const std::wstring& ipAddress, const int port,
                                               ConnectionCallbackType connectionCallback) const
{
    const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

    return impl->operations.createConnectAsynchronousOperation(ipAddress, port, std::move(connectionCallback));
}

ProtocolSequenceNumber SocketServiceProvider::generateSequenceNumber() const
{
    const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

    return impl->sequencer.generate();
}

void SocketServiceProvider::sendBytes(const SocketServiceId serviceId, std::vector<uint8_t>&& bytes) const
{
    if (serviceId != InvalidServiceId)
    {
        const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

        impl->operations.createSendBytesAsynchronousOperation(serviceId, std::move(bytes));
    }
    else
    {
        THROW(std::logic_error, "Cannot send bytes using an InvalidServiceId");
    }
}

void SocketServiceProvider::sendRequest(const SocketServiceId serviceId, const ProtocolSequenceNumber sequenceNumber,
                                        std::vector<uint8_t>&& bytes,
                                        Function<void(std::any&&)>&& expectedResponseConsumer) const
{
    if (serviceId != InvalidServiceId)
    {
        const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

        impl->operations.createSendRequestAsynchronousOperation(serviceId, sequenceNumber, std::move(bytes),
                                                                std::move(expectedResponseConsumer));
    }
    else
    {
        THROW(std::logic_error, "Cannot send bytes using an InvalidServiceId");
    }
}

void SocketServiceProvider::registerMessageConsumer(const SocketServiceId serviceId,
                                                    const ProtocolIdentifier protocolIdentifier,
                                                    Function<void(std::any&&)>&& messageConsumer) const
{
    if (serviceId != InvalidServiceId)
    {
        const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

        impl->operations.createRegisterMessageConsumerAsynchronousOperation(serviceId, protocolIdentifier,
                                                                            std::move(messageConsumer));
    }
    else
    {
        THROW(std::logic_error, "Cannot register a message consumer using an InvalidServiceId");
    }
}

void SocketServiceProvider::registerRequestCallback(const SocketServiceId serviceId,
                                                    const ProtocolIdentifier protocolIdentifier,
                                                    Function<std::any(std::any&&)>&& requestCallback) const
{
    if (serviceId != InvalidServiceId)
    {
        const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

        impl->operations.createRegisterRequestCallbackAsynchronousOperation(serviceId, protocolIdentifier,
                                                                            std::move(requestCallback));
    }
    else
    {
        THROW(std::logic_error, "Cannot register a request consumer using an InvalidServiceId");
    }
}

void SocketServiceProvider::close(const SocketServiceId serviceId) const
{
    if (serviceId != InvalidServiceId)
    {
        const auto impl = static_cast<SocketServiceProviderImplementation*>(implementation_.get());

        impl->operations.createCloseAsynchronousOperation(serviceId);
    }
}

}
