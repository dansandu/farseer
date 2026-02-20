#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/socket_service.hpp"
#include "dansandu/journey/logging.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <winsock2.h>

namespace dansandu::farseer::internal::windows::asynchronous_operation
{

constexpr auto initialCompletionKey = InvalidServiceId.getUnderlying();

class IAsynchronousOperationsRegistry
{
public:
    IAsynchronousOperationsRegistry() = default;

    IAsynchronousOperationsRegistry(const IAsynchronousOperationsRegistry& other) = delete;
    IAsynchronousOperationsRegistry(IAsynchronousOperationsRegistry&& other) noexcept = delete;
    IAsynchronousOperationsRegistry& operator=(const IAsynchronousOperationsRegistry& other) = delete;
    IAsynchronousOperationsRegistry& operator=(IAsynchronousOperationsRegistry&& other) noexcept = delete;

    virtual ~IAsynchronousOperationsRegistry() noexcept
    {
    }

    virtual void createAcceptAsynchronousOperation(const SocketServiceId listeningServiceId) = 0;

    virtual void createReceiveAsynchronousOperation(const SocketServiceId serviceId) = 0;

    virtual void createSendBytesAsynchronousOperation(const SocketServiceId serviceId,
                                                      std::vector<uint8_t>&& bytes) = 0;
};

class AsynchronousOperation
{
public:
    explicit AsynchronousOperation(const SocketServiceId serviceId) : serviceId_{serviceId}, overlapped_{}
    {
        SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));
    }

    AsynchronousOperation() = delete;
    AsynchronousOperation(const AsynchronousOperation& other) = delete;
    AsynchronousOperation(AsynchronousOperation&& other) noexcept = delete;
    AsynchronousOperation& operator=(const AsynchronousOperation& other) = delete;
    AsynchronousOperation& operator=(AsynchronousOperation&& other) noexcept = delete;

    virtual ~AsynchronousOperation() noexcept
    {
    }

    virtual void
    postToCompletionPort(dansandu::farseer::internal::windows::socket_service::SocketServiceContainer& services,
                         IAsynchronousOperationsRegistry& asynchronousOperationsRegistry,
                         const HANDLE completionPort) = 0;

    virtual bool finalize(dansandu::farseer::internal::sequencer::Sequencer<SocketServiceId>& sequencer,
                          dansandu::farseer::internal::windows::socket_service::SocketServiceContainer& services,
                          IAsynchronousOperationsRegistry& asynchronousOperationsRegistry, const HANDLE completionPort,
                          const DWORD numberOfBytesTransferred) = 0;

    virtual const char* getName() const = 0;

    virtual dansandu::journey::Level reinterpretSystemErrorCode(const DWORD errorCode) const
    {
        return dansandu::journey::Level::error;
    }

    SocketServiceId getServiceId() const
    {
        return serviceId_;
    }

    LPWSAOVERLAPPED getOverlapped()
    {
        return &overlapped_;
    }

protected:
    SocketServiceId serviceId_;
    WSAOVERLAPPED overlapped_;
};

class AsynchronousOperationContainer : public IAsynchronousOperationsRegistry
{
public:
    AsynchronousOperationContainer();

    ~AsynchronousOperationContainer() noexcept;

    SocketServiceId createConnectAsynchronousOperation(const std::wstring& ipAddress, const int port,
                                                       ConnectionCallbackType connectionCallback);

    SocketServiceId createListenAsynchronousOperation(const std::wstring& ipAddress, const int port,
                                                      ConnectionCallbackType connectionCallback);

    void createAcceptAsynchronousOperation(const SocketServiceId listeningServiceId) override;

    void createReceiveAsynchronousOperation(const SocketServiceId serviceId) override;

    void createSendBytesAsynchronousOperation(const SocketServiceId serviceId, std::vector<uint8_t>&& bytes) override;

    void createSendRequestAsynchronousOperation(const SocketServiceId serviceId,
                                                const ProtocolSequenceNumber sequenceNumber,
                                                std::vector<uint8_t>&& bytes,
                                                Function<void(std::any&&)>&& expectedResponseConsumer);

    void createRegisterMessageConsumerAsynchronousOperation(const SocketServiceId serviceId,
                                                            const ProtocolIdentifier protocolIdentifier,
                                                            Function<void(std::any&&)>&& messageConsumer);

    void createRegisterRequestCallbackAsynchronousOperation(const SocketServiceId serviceId,
                                                            const ProtocolIdentifier protocolIdentifier,
                                                            Function<std::any(std::any&&)>&& requestConsumer);

    void createCloseAsynchronousOperation(const SocketServiceId serviceId);

    void createAbortAsynchronousOperation();

    bool waitAndConsumeAsynchronousOperation();

private:
    SocketServiceId insertOperation(std::unique_ptr<AsynchronousOperation> operation);

    void handleSuccessfulAsynchronousOperation(const LPWSAOVERLAPPED overlapped, const DWORD numberOfBytesTransferred);

    void handleFailedAsynchronousOperation(const LPWSAOVERLAPPED overlapped, const DWORD errorCode);

    void handleFailedAsynchronousOperation(const LPWSAOVERLAPPED overlapped, const std::wstring_view message);

    const HANDLE completionPort_;
    dansandu::farseer::internal::sequencer::Sequencer<SocketServiceId> serviceIdSequencer_;
    dansandu::farseer::internal::windows::socket_service::SocketServiceContainer socketServiceContainer_;
    std::map<LPWSAOVERLAPPED, std::unique_ptr<AsynchronousOperation>> operations_;
    mutable std::recursive_mutex operationsMutex_;
};

}
