#pragma once

#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/operation.hpp"

#include <map>
#include <memory>
#include <thread>
#include <vector>

namespace dansandu::farseer::internal::windows::operation_scheduler
{

class OperationScheduler : public dansandu::farseer::internal::windows::operation::IOperationScheduler
{
public:
    OperationScheduler();

    ~OperationScheduler() noexcept;

    HANDLE getCompletionPort() override;

    void scheduleAcceptSystemOperation(const SocketIdentifier listeningSocketIdentifier) override;

    void scheduleReceiveSystemOperation(const SocketIdentifier socketIdentifier) override;

    void
    scheduleSendBytesSystemOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) override;

    SocketIdentifier scheduleConnectUserOperation(
        const std::string& ipAddress, const int port,
        UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
    );

    SocketIdentifier scheduleListenUserOperation(
        const std::string& ipAddress, const int port,
        UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
    );

    void scheduleSendBytesUserOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes);

    void scheduleSendRequestUserOperation(
        const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber protocolSequenceNumber,
        std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer
    );

    void scheduleRegisterMessageConsumerUserOperation(
        const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
        UniqueFunction<void(std::any&&)>&& messageConsumer
    );

    void scheduleRegisterRequestCallbackUserOperation(
        const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
        UniqueFunction<std::any(std::any&&)>&& requestConsumer
    );

    void scheduleCloseUserOperation(const SocketIdentifier socketIdentifier);

private:
    void scheduleAbortOperation();

    void
    scheduleUserOperation(std::unique_ptr<dansandu::farseer::internal::windows::operation::IUserOperation>&& operation);

    void scheduleSystemOperation(
        std::unique_ptr<dansandu::farseer::internal::windows::operation::ISystemOperation>&& operation
    );

    bool handleSuccessfulOperation(
        dansandu::farseer::internal::windows::operation::IOperation& operation, const DWORD numberOfBytesTransferred
    );

    void handleFailedOperation(
        dansandu::farseer::internal::windows::operation::IOperation& operation, const DWORD errorCode
    );

    void handleOperationExecutionFailure(
        dansandu::farseer::internal::windows::operation::IOperation& operation, const std::wstring_view message = {},
        const bool quietDiscard = false
    );

    void invokeOperationFailureHook(
        dansandu::farseer::internal::windows::operation::IOperation& operation, const DWORD errorCode
    );

    void consumeOperationsWork();

    void consumeOperations();

    void drainOperations();

    const HANDLE completionPort_;
    dansandu::farseer::internal::sequencer::Sequencer<SocketIdentifier> socketIdentifierSequencer_;
    dansandu::farseer::internal::windows::socket_container::SocketContainer socketContainer_;
    std::atomic<std::size_t> state_;
    std::thread thread_;
};

}
