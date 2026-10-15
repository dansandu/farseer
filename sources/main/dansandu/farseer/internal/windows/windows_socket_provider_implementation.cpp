#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/windows_socket_provider_implementation.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/socket_provider_implementation.hpp"
#include "dansandu/farseer/internal/windows/operation_scheduler.hpp"
#include "dansandu/farseer/internal/windows/wsa_scope_guard.hpp"
#include "dansandu/journey/logging.hpp"

#include <memory>
#include <string>
#include <vector>

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::socket_provider_implementation::ISocketProviderImplementation;
using dansandu::farseer::internal::windows::operation_scheduler::OperationScheduler;
using dansandu::farseer::internal::windows::wsa_scope_guard::WsaScopeGuard;

namespace dansandu::farseer::internal::windows::windows_socket_provider_implementation
{

namespace
{

class WindowsSocketProviderImplementation : public ISocketProviderImplementation
{
public:
    explicit WindowsSocketProviderImplementation(const bool initializeWsa) : wsaScopeGuard_{initializeWsa}
    {
    }

    SocketIdentifier listen(
        const std::string& ipAddress, const int port,
        UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
    ) override
    {
        return operationScheduler_.scheduleListenUserOperation(ipAddress, port, std::move(connectionCallback));
    }

    SocketIdentifier connect(
        const std::string& ipAddress, const int port,
        UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
    ) override
    {
        return operationScheduler_.scheduleConnectUserOperation(ipAddress, port, std::move(connectionCallback));
    }

    void sendBytes(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) override
    {
        operationScheduler_.scheduleSendBytesUserOperation(socketIdentifier, std::move(bytes));
    }

    void sendRequest(
        const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
        std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer
    ) override
    {
        operationScheduler_.scheduleSendRequestUserOperation(
            socketIdentifier, sequenceNumber, std::move(bytes), std::move(responseConsumer)
        );
    }

    void registerMessageConsumer(
        const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
        UniqueFunction<void(std::any&&)>&& messageConsumer
    ) override
    {
        operationScheduler_.scheduleRegisterMessageConsumerUserOperation(
            socketIdentifier, protocolIdentifier, std::move(messageConsumer)
        );
    }

    void registerRequestCallback(
        const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
        UniqueFunction<std::any(std::any&&)>&& requestCallback
    ) override
    {
        operationScheduler_.scheduleRegisterRequestCallbackUserOperation(
            socketIdentifier, protocolIdentifier, std::move(requestCallback)
        );
    }

    void close(const SocketIdentifier socketIdentifier) override
    {
        operationScheduler_.scheduleCloseUserOperation(socketIdentifier);
    }

    ProtocolSequenceNumber generateSequenceNumber() override
    {
        return protocolSequencer_.generate();
    }

private:
    const WsaScopeGuard wsaScopeGuard_;
    Sequencer<ProtocolSequenceNumber> protocolSequencer_;
    OperationScheduler operationScheduler_;
};

}

std::shared_ptr<dansandu::farseer::internal::socket_provider_implementation::ISocketProviderImplementation>
createWindowsSocketProviderImplementation(const bool initializeWsa)
{
    return std::make_shared<WindowsSocketProviderImplementation>(initializeWsa);
}

}
#endif
