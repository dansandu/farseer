#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

namespace dansandu::farseer::internal::windows::operation
{

constexpr auto defaultCompletionKey = invalidSocketIdentifier.getUnderlying();
constexpr auto maximumReceiveBufferSize = 4096;

struct Socket
{
    dansandu::farseer::internal::windows::windows_socket::WindowsSocket socket;
    dansandu::farseer::internal::protocol_reader::ProtocolReader protocolReader;
    SocketIdentifier listeningSocketIdentifier;
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)> connectionCallback;
    std::vector<uint8_t> inboundBytes;
    // The outbound bytes buffer is part of the socket to reuse capacity and reduce allocations.
    std::vector<uint8_t> outboundBytes;
};

class IOperationScheduler
{
public:
    IOperationScheduler(const IOperationScheduler& other) = delete;
    IOperationScheduler(IOperationScheduler&& other) noexcept = delete;
    IOperationScheduler& operator=(const IOperationScheduler& other) = delete;
    IOperationScheduler& operator=(IOperationScheduler&& other) noexcept = delete;

    IOperationScheduler() = default;

    virtual ~IOperationScheduler() noexcept = default;

    virtual HANDLE getCompletionPort() = 0;

    virtual Socket& insertSocket(const SocketIdentifier socketIdentifier, Socket&& socket) = 0;

    virtual Socket& getSocketOrThrow(const SocketIdentifier socketIdentifier) = 0;

    virtual void eraseSocket(const SocketIdentifier socketIdentifier) = 0;

    virtual void scheduleAcceptOperation(const SocketIdentifier listeningSocketIdentifier) = 0;

    virtual void scheduleReceiveOperation(const SocketIdentifier socketIdentifier) = 0;

    virtual void scheduleSendBytesOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) = 0;
};

class IOperation : public WSAOVERLAPPED
{
public:
    IOperation(const IOperation& other) = delete;
    IOperation(IOperation&& other) noexcept = delete;
    IOperation& operator=(const IOperation& other) = delete;
    IOperation& operator=(IOperation&& other) noexcept = delete;

    IOperation()
    {
        eraseWsaOverlapped();
    }

    virtual ~IOperation() noexcept = default;

    virtual const char* getName() const = 0;

    virtual SocketIdentifier getSocketIdentifier() const = 0;

    virtual dansandu::journey::Level getLoggingLevelFromErrorCode(const DWORD errorCode) const = 0;

    // Always called on the operation consumer thread. Should be safe to access consumer thread resources without locks
    // inside this method. If the method returns true, then the operation can be discarded. Otherwise, the operation
    // was rescheduled and will be kept alive for another execution.
    virtual bool execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) = 0;

    void eraseWsaOverlapped()
    {
        SecureZeroMemory(static_cast<WSAOVERLAPPED*>(this), sizeof(WSAOVERLAPPED));
    }
};

class INonUserOperation : public IOperation
{
public:
    INonUserOperation() = default;

    virtual ~INonUserOperation() noexcept = default;

    // These operations are not scheduled directly by farseer users. Instead, these operations are spawned by other
    // operations through their IOperation::execute implementation. Therefore, it should be safe to access consumer
    // thread resources without locks inside this method.
    virtual void schedule(IOperationScheduler& operationScheduler) = 0;
};

}
