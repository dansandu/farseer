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

    dansandu::farseer::internal::windows::operation::Socket& insertSocket(
        const SocketIdentifier socketIdentifier, dansandu::farseer::internal::windows::operation::Socket&& socket
    ) override;

    dansandu::farseer::internal::windows::operation::Socket&
    getSocketOrThrow(const SocketIdentifier socketIdentifier) override;

    void eraseSocket(const SocketIdentifier socketIdentifier) override;

    SocketIdentifier scheduleConnectOperation(
        const std::string& ipAddress, const int port,
        UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
    );

    SocketIdentifier scheduleListenOperation(
        const std::string& ipAddress, const int port,
        UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
    );

    void scheduleAcceptOperation(const SocketIdentifier listeningSocketIdentifier) override;

    void scheduleReceiveOperation(const SocketIdentifier socketIdentifier) override;

    void scheduleSendBytesOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) override;

    void scheduleSendRequestOperation(
        const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber protocolSequenceNumber,
        std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer
    );

    void scheduleRegisterMessageConsumerOperation(
        const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
        UniqueFunction<void(std::any&&)>&& messageConsumer
    );

    void scheduleRegisterRequestCallbackOperation(
        const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
        UniqueFunction<std::any(std::any&&)>&& requestConsumer
    );

    void scheduleCloseOperation(const SocketIdentifier socketIdentifier);

private:
    void scheduleAbortOperation();

    void scheduleOperation(std::unique_ptr<dansandu::farseer::internal::windows::operation::IOperation>&& operation);

    bool handleSuccessfulOperation(
        const std::unique_ptr<dansandu::farseer::internal::windows::operation::IOperation>& operation,
        const DWORD numberOfBytesTransferred
    );

    void handleFailedOperation(
        std::unique_ptr<dansandu::farseer::internal::windows::operation::IOperation>&& operation, const DWORD errorCode
    );

    void handleOperationExecutionFailure(
        const char* const name, const SocketIdentifier socketIdentifier, const std::wstring_view message = {}
    );
    void consumeOperationsWork();

    void consumeOperations();

    const HANDLE completionPort_;
    dansandu::farseer::internal::sequencer::Sequencer<SocketIdentifier> socketIdentifierSequencer_;
    std::map<SocketIdentifier, dansandu::farseer::internal::windows::operation::Socket> sockets_;
    std::thread thread_;
};

}
