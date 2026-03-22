#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

namespace dansandu::farseer::internal::windows::i_operation_scheduler
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
    IOperationScheduler(const IOperationScheduler& other) = delete;
    IOperationScheduler(IOperationScheduler&& other) noexcept = delete;
    IOperationScheduler& operator=(const IOperationScheduler& other) = delete;
    IOperationScheduler& operator=(IOperationScheduler&& other) noexcept = delete;

    IOperationScheduler() = default;

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
    Operation() = delete;
    Operation(const Operation& other) = delete;
    Operation(Operation&& other) noexcept = delete;
    Operation& operator=(const Operation& other) = delete;
    Operation& operator=(Operation&& other) noexcept = delete;

    explicit Operation(const SocketIdentifier socketIdentifier) : socketIdentifier_{socketIdentifier}, overlapped_{}
    {
        SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));
    }

    virtual ~Operation() noexcept
    {
    }

    SocketIdentifier getSocketIdentifier() const
    {
        return socketIdentifier_;
    }

    LPWSAOVERLAPPED getOverlapped()
    {
        return &overlapped_;
    }

    virtual const char* getName() const = 0;

    virtual dansandu::journey::Level getSystemErrorCodeLevel(const DWORD errorCode) const
    {
        return dansandu::journey::Level::error;
    }

    virtual bool discard(const DWORD numberOfBytesTransferred) const = 0;

    virtual void postToCompletionPort(IOperationScheduler& operationScheduler) = 0;

    virtual void execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) = 0;

protected:
    SocketIdentifier socketIdentifier_;
    WSAOVERLAPPED overlapped_;
};

}
