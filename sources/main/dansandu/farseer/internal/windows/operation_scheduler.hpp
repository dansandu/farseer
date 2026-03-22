#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"
#include "dansandu/journey/logging.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <winsock2.h>

namespace dansandu::farseer::internal::windows::operation_scheduler
{

constexpr auto defaultCompletionKey = invalidSocketIdentifier.getUnderlying();

struct Socket
{
    dansandu::farseer::internal::windows::windows_socket::WindowsSocket socket;
    dansandu::farseer::internal::protocol_reader::ProtocolReader protocolReader;
    SocketIdentifier listeningSocketIdentifier;
    ConnectionCallback connectionCallback;
};

class IOperationScheduler
{
public:
    IOperationScheduler() = default;

    IOperationScheduler(const IOperationScheduler& other) = delete;
    IOperationScheduler(IOperationScheduler&& other) noexcept = delete;
    IOperationScheduler& operator=(const IOperationScheduler& other) = delete;
    IOperationScheduler& operator=(IOperationScheduler&& other) noexcept = delete;

    virtual ~IOperationScheduler() noexcept
    {
    }

    virtual HANDLE getCompletionPort() = 0;

    virtual Socket& insertSocket(const SocketIdentifier socketIdentifier, Socket&& socket) = 0;

    virtual Socket& getSocketOrThrow(const SocketIdentifier socketIdentifier) = 0;

    virtual void eraseSocket(const SocketIdentifier socketIdentifier) = 0;

    virtual void scheduleAcceptOperation(const SocketIdentifier listeningSocketIdentifier) = 0;

    virtual void scheduleReceiveOperation(const SocketIdentifier socketIdentifier) = 0;

    virtual void scheduleSendBytesOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) = 0;
};

class Operation
{
public:
    explicit Operation(const SocketIdentifier socketIdentifier) : socketIdentifier_{socketIdentifier}, overlapped_{}
    {
        SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));
    }

    Operation() = delete;
    Operation(const Operation& other) = delete;
    Operation(Operation&& other) noexcept = delete;
    Operation& operator=(const Operation& other) = delete;
    Operation& operator=(Operation&& other) noexcept = delete;

    virtual ~Operation() noexcept
    {
    }

    virtual void postToCompletionPort(IOperationScheduler& operationScheduler) = 0;

    virtual bool finalize(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) = 0;

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

class OperationScheduler : public IOperationScheduler
{
public:
    OperationScheduler();

    ~OperationScheduler() noexcept;

    HANDLE getCompletionPort() override;

    Socket& insertSocket(const SocketIdentifier socketIdentifier, Socket&& socket) override;

    Socket& getSocketOrThrow(const SocketIdentifier socketIdentifier) override;

    void eraseSocket(const SocketIdentifier socketIdentifier) override;

    SocketIdentifier scheduleConnectOperation(const std::string& ipAddress, const int port,
                                              ConnectionCallback&& connectionCallback);

    SocketIdentifier scheduleListenOperation(const std::string& ipAddress, const int port,
                                             ConnectionCallback&& connectionCallback);

    void scheduleAcceptOperation(const SocketIdentifier listeningSocketIdentifier) override;

    void scheduleReceiveOperation(const SocketIdentifier socketIdentifier) override;

    void scheduleSendBytesOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) override;

    void scheduleSendRequestOperation(const SocketIdentifier socketIdentifier,
                                      const ProtocolSequenceNumber protocolSequenceNumber, std::vector<uint8_t>&& bytes,
                                      UniqueFunction<void(std::any&&)>&& expectedResponseConsumer);

    void scheduleRegisterMessageConsumerOperation(const SocketIdentifier socketIdentifier,
                                                  const ProtocolIdentifier protocolIdentifier,
                                                  UniqueFunction<void(std::any&&)>&& messageConsumer);

    void scheduleRegisterRequestCallbackOperation(const SocketIdentifier socketIdentifier,
                                                  const ProtocolIdentifier protocolIdentifier,
                                                  UniqueFunction<std::any(std::any&&)>&& requestConsumer);

    void scheduleCloseOperation(const SocketIdentifier socketIdentifier);

private:
    void scheduleAbortOperation();

    void consumeOperations();

    void insertOperation(std::unique_ptr<Operation>&& operation);

    void handleSuccessfulOperation(const LPWSAOVERLAPPED overlapped, const DWORD numberOfBytesTransferred);

    void handleFailedOperation(const LPWSAOVERLAPPED overlapped, const DWORD errorCode);

    void handleFailedOperation(const LPWSAOVERLAPPED overlapped, const std::wstring_view message);

    const HANDLE completionPort_;
    dansandu::farseer::internal::sequencer::Sequencer<SocketIdentifier> socketIdentifierSequencer_;
    std::map<SocketIdentifier, Socket> sockets_;
    std::map<LPWSAOVERLAPPED, std::unique_ptr<Operation>> operations_;
    std::recursive_mutex operationsMutex_;
    std::thread thread_;
};

}
