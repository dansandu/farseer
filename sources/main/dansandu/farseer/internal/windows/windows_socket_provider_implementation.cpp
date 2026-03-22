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

    SocketIdentifier listen(const std::string& ipAddress, const int port,
                            ConnectionCallback&& connectionCallback) override
    {
        return operationScheduler_.scheduleListenOperation(ipAddress, port, std::move(connectionCallback));
    }

    SocketIdentifier connect(const std::string& ipAddress, const int port,
                             ConnectionCallback&& connectionCallback) override
    {
        return operationScheduler_.scheduleConnectOperation(ipAddress, port, std::move(connectionCallback));
    }

    ProtocolSequenceNumber generateSequenceNumber() override
    {
        return protocolSequencer_.generate();
    }

    void sendBytes(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) override
    {
        operationScheduler_.scheduleSendBytesOperation(socketIdentifier, std::move(bytes));
    }

    void sendRequest(const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
                     std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& expectedResponseConsumer) override
    {
        operationScheduler_.scheduleSendRequestOperation(socketIdentifier, sequenceNumber, std::move(bytes),
                                                         std::move(expectedResponseConsumer));
    }

    void registerMessageConsumer(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                 UniqueFunction<void(std::any&&)>&& messageConsumer) override
    {
        operationScheduler_.scheduleRegisterMessageConsumerOperation(socketIdentifier, protocolIdentifier,
                                                                     std::move(messageConsumer));
    }

    void registerRequestCallback(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                 UniqueFunction<std::any(std::any&&)>&& requestCallback) override
    {
        operationScheduler_.scheduleRegisterRequestCallbackOperation(socketIdentifier, protocolIdentifier,
                                                                     std::move(requestCallback));
    }

    void close(const SocketIdentifier socketIdentifier) override
    {
        operationScheduler_.scheduleCloseOperation(socketIdentifier);
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
