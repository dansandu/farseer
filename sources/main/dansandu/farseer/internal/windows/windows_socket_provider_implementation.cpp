#include "dansandu/farseer/internal/windows/windows_socket_provider_implementation.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/socket_provider_implementation.hpp"
#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/wsa_scope_guard.hpp"
#include "dansandu/journey/logging.hpp"

#include <string>
#include <vector>

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::socket_provider_implementation::ISocketProviderImplementation;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperationScheduler;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::wsa_scope_guard::WsaScopeGuard;

namespace dansandu::farseer::internal::windows::windows_socket_provider_implementation
{

namespace
{

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

}

class WindowsSocketProviderImplementation : public ISocketProviderImplementation
{
public:
    explicit WindowsSocketProviderImplementation(const bool initializeWsa)
        : wsaScopeGuard_{initializeWsa},
          operations_{},
          thread_{createAsynchronousOperationsConsumerThread(&operations_)}
    {
    }

    ~WindowsSocketProviderImplementation() noexcept
    {
        operations_.createAbortAsynchronousOperation();

        const auto waitTimeout = INFINITE;
        ::WaitForSingleObject(thread_, waitTimeout);
        ::CloseHandle(thread_);
    }

    SocketIdentifier listen(const std::wstring& ipAddress, const int port,
                            ConnectionCallback&& connectionCallback) override
    {
        return operations_.createListenAsynchronousOperation(ipAddress, port, std::move(connectionCallback));
    }

    SocketIdentifier connect(const std::wstring& ipAddress, const int port,
                             ConnectionCallback&& connectionCallback) override
    {
        return operations_.createConnectAsynchronousOperation(ipAddress, port, std::move(connectionCallback));
    }

    ProtocolSequenceNumber generateSequenceNumber() override
    {
        return sequencer_.generate();
    }

    void sendBytes(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) override
    {
        operations_.createSendBytesAsynchronousOperation(socketIdentifier, std::move(bytes));
    }

    void sendRequest(const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
                     std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& expectedResponseConsumer) override
    {
        operations_.createSendRequestAsynchronousOperation(socketIdentifier, sequenceNumber, std::move(bytes),
                                                           std::move(expectedResponseConsumer));
    }

    void registerMessageConsumer(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                 UniqueFunction<void(std::any&&)>&& messageConsumer) override
    {
        operations_.createRegisterMessageConsumerAsynchronousOperation(socketIdentifier, protocolIdentifier,
                                                                       std::move(messageConsumer));
    }

    void registerRequestCallback(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                 UniqueFunction<std::any(std::any&&)>&& requestCallback) override
    {
        operations_.createRegisterRequestCallbackAsynchronousOperation(socketIdentifier, protocolIdentifier,
                                                                       std::move(requestCallback));
    }

    void close(const SocketIdentifier socketIdentifier) override
    {
        operations_.createCloseAsynchronousOperation(socketIdentifier);
    }

private:
    const WsaScopeGuard wsaScopeGuard_;
    AsynchronousOperationScheduler operations_;
    Sequencer<ProtocolSequenceNumber> sequencer_;
    const HANDLE thread_;
};

std::shared_ptr<dansandu::farseer::internal::socket_provider_implementation::ISocketProviderImplementation>
createWindowsSocketProviderImplementation(const bool initializeWsa)
{
    return std::make_shared<WindowsSocketProviderImplementation>(initializeWsa);
}

}
