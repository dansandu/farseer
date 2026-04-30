#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

namespace dansandu::farseer::internal::windows::operation
{

constexpr auto defaultCompletionKey = invalidSocketIdentifier.getUnderlying();

struct Socket
{
    dansandu::farseer::internal::windows::windows_socket::WindowsSocket socket;
    dansandu::farseer::internal::protocol_reader::ProtocolReader protocolReader;
    SocketIdentifier listeningSocketIdentifier;
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)> connectionCallback;
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

class IOperation
{
public:
    IOperation(const IOperation& other) = delete;
    IOperation(IOperation&& other) noexcept = delete;
    IOperation& operator=(const IOperation& other) = delete;
    IOperation& operator=(IOperation&& other) noexcept = delete;

    IOperation() = default;

    virtual ~IOperation() noexcept = default;

    virtual const char* getName() const = 0;

    virtual SocketIdentifier getSocketIdentifier() const = 0;

    virtual dansandu::journey::Level getSystemErrorCodeLevel(const DWORD errorCode) const = 0;

    virtual bool discard(const DWORD numberOfBytesTransferred) const = 0;

    virtual LPWSAOVERLAPPED getOverlapped() = 0;

    virtual void postToCompletionPort(IOperationScheduler& operationScheduler) = 0;

    virtual void execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) = 0;
};

}
