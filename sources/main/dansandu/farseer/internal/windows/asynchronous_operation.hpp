#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"
#include "dansandu/journey/logging.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <winsock2.h>

namespace dansandu::farseer::internal::windows::asynchronous_operation
{

constexpr auto defaultCompletionKey = invalidSocketIdentifier.getUnderlying();

struct Socket
{
    dansandu::farseer::internal::windows::windows_socket::WindowsSocket socket;
    dansandu::farseer::internal::protocol_reader::ProtocolReader protocolReader;
    SocketIdentifier listeningSocketIdentifier;
    ConnectionCallback connectionCallback;
};

class IAsynchronousOperationsScheduler
{
public:
    IAsynchronousOperationsScheduler() = default;

    IAsynchronousOperationsScheduler(const IAsynchronousOperationsScheduler& other) = delete;
    IAsynchronousOperationsScheduler(IAsynchronousOperationsScheduler&& other) noexcept = delete;
    IAsynchronousOperationsScheduler& operator=(const IAsynchronousOperationsScheduler& other) = delete;
    IAsynchronousOperationsScheduler& operator=(IAsynchronousOperationsScheduler&& other) noexcept = delete;

    virtual ~IAsynchronousOperationsScheduler() noexcept
    {
    }

    virtual HANDLE getCompletionPort() = 0;

    virtual Socket& insertSocket(const SocketIdentifier socketIdentifier, Socket&& socket) = 0;

    virtual Socket& getSocketOrThrow(const SocketIdentifier socketIdentifier) = 0;

    virtual void eraseSocket(const SocketIdentifier socketIdentifier) = 0;

    virtual void createAcceptAsynchronousOperation(const SocketIdentifier listeningSocketIdentifier) = 0;

    virtual void createReceiveAsynchronousOperation(const SocketIdentifier socketIdentifier) = 0;

    virtual void createSendBytesAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                                      std::vector<uint8_t>&& bytes) = 0;
};

class AsynchronousOperation
{
public:
    explicit AsynchronousOperation(const SocketIdentifier socketIdentifier)
        : socketIdentifier_{socketIdentifier}, overlapped_{}
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

    virtual void postToCompletionPort(IAsynchronousOperationsScheduler& asynchronousOperationsScheduler) = 0;

    virtual bool finalize(IAsynchronousOperationsScheduler& asynchronousOperationsScheduler,
                          const DWORD numberOfBytesTransferred) = 0;

    virtual const char* getName() const = 0;

    virtual dansandu::journey::Level getSystemErrorCodeLevel(const DWORD errorCode) const
    {
        return dansandu::journey::Level::error;
    }

    SocketIdentifier getSocketIdentifier() const
    {
        return socketIdentifier_;
    }

    LPWSAOVERLAPPED getOverlapped()
    {
        return &overlapped_;
    }

protected:
    SocketIdentifier socketIdentifier_;
    WSAOVERLAPPED overlapped_;
};

class AsynchronousOperationScheduler : public IAsynchronousOperationsScheduler
{
public:
    AsynchronousOperationScheduler();

    ~AsynchronousOperationScheduler() noexcept;

    HANDLE getCompletionPort() override;

    Socket& insertSocket(const SocketIdentifier socketIdentifier, Socket&& socket) override;

    Socket& getSocketOrThrow(const SocketIdentifier socketIdentifier) override;

    void eraseSocket(const SocketIdentifier socketIdentifier) override;

    SocketIdentifier createConnectAsynchronousOperation(const std::wstring& ipAddress, const int port,
                                                        ConnectionCallback&& connectionCallback);

    SocketIdentifier createListenAsynchronousOperation(const std::wstring& ipAddress, const int port,
                                                       ConnectionCallback&& connectionCallback);

    void createAcceptAsynchronousOperation(const SocketIdentifier listeningSocketIdentifier) override;

    void createReceiveAsynchronousOperation(const SocketIdentifier socketIdentifier) override;

    void createSendBytesAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                              std::vector<uint8_t>&& bytes) override;

    void createSendRequestAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                                const ProtocolSequenceNumber protocolSequenceNumber,
                                                std::vector<uint8_t>&& bytes,
                                                UniqueFunction<void(std::any&&)>&& expectedResponseConsumer);

    void createRegisterMessageConsumerAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                                            const ProtocolIdentifier protocolIdentifier,
                                                            UniqueFunction<void(std::any&&)>&& messageConsumer);

    void createRegisterRequestCallbackAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                                            const ProtocolIdentifier protocolIdentifier,
                                                            UniqueFunction<std::any(std::any&&)>&& requestConsumer);

    void createCloseAsynchronousOperation(const SocketIdentifier socketIdentifier);

    void createAbortAsynchronousOperation();

    bool waitAndConsumeAsynchronousOperation();

private:
    void insertOperation(std::unique_ptr<AsynchronousOperation> operation);

    void handleSuccessfulAsynchronousOperation(const LPWSAOVERLAPPED overlapped, const DWORD numberOfBytesTransferred);

    void handleFailedAsynchronousOperation(const LPWSAOVERLAPPED overlapped, const DWORD errorCode);

    void handleFailedAsynchronousOperation(const LPWSAOVERLAPPED overlapped, const std::wstring_view message);

    const HANDLE completionPort_;
    dansandu::farseer::internal::sequencer::Sequencer<SocketIdentifier> socketIdentifierSequencer_;
    std::map<SocketIdentifier, Socket> sockets_;
    std::map<LPWSAOVERLAPPED, std::unique_ptr<AsynchronousOperation>> operations_;
    mutable std::recursive_mutex operationsMutex_;
};

}
