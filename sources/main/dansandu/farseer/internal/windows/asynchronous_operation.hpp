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

constexpr auto initialCompletionKey = InvalidServiceId.getInteger();

class IAsynchronousOperationsFactory;

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
                         const HANDLE completionPort) = 0;

    virtual bool finalize(dansandu::farseer::internal::sequencer::Sequencer<SocketServiceId>& sequencer,
                          dansandu::farseer::internal::windows::socket_service::SocketServiceContainer& services,
                          IAsynchronousOperationsFactory& asynchronousOperationsFactory, const HANDLE completionPort,
                          const DWORD numberOfBytesTransferred) = 0;

    virtual const char* getName() const = 0;

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

class IAsynchronousOperationsFactory
{
public:
    IAsynchronousOperationsFactory() = default;

    IAsynchronousOperationsFactory(const IAsynchronousOperationsFactory& other) = delete;

    IAsynchronousOperationsFactory(IAsynchronousOperationsFactory&& other) noexcept = delete;

    IAsynchronousOperationsFactory& operator=(const IAsynchronousOperationsFactory& other) = delete;

    IAsynchronousOperationsFactory& operator=(IAsynchronousOperationsFactory&& other) noexcept = delete;

    virtual ~IAsynchronousOperationsFactory() noexcept
    {
    }

    virtual void createAcceptAsynchronousOperation(const SocketServiceId listeningServiceId) = 0;

    virtual void createReceiveAsynchronousOperation(const SocketServiceId serviceId) = 0;
};

class AsynchronousOperationContainer : public IAsynchronousOperationsFactory
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

    void createRegisterMessageConsumerAsynchronousOperation(const SocketServiceId serviceId,
                                                            const ProtocolIdentifier protocolIdentifier,
                                                            std::function<void(std::any)> messageConsumer);

    void createSendBytesAsynchronousOperation(const SocketServiceId serviceId, std::vector<uint8_t> bytes);

    void createCloseAsynchronousOperation(const SocketServiceId serviceId);

    void createAbortAsynchronousOperation();

    bool waitAndConsumeAsynchronousOperation();

private:
    SocketServiceId insertOperation(std::unique_ptr<AsynchronousOperation> operation);

    void handleSuccessfulAsynchronousOperation(const LPWSAOVERLAPPED overlapped, const DWORD numberOfBytesTransferred);

    void handleFailedAsynchronousOperation(const LPWSAOVERLAPPED overlapped);

    const HANDLE completionPort_;
    dansandu::farseer::internal::sequencer::Sequencer<SocketServiceId> serviceIdSequencer_;
    dansandu::farseer::internal::windows::socket_service::SocketServiceContainer socketServiceContainer_;
    std::map<LPWSAOVERLAPPED, std::unique_ptr<AsynchronousOperation>> operations_;
    mutable std::recursive_mutex operationsMutex_;
};

}
