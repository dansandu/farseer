#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/windows/socket_container.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

namespace dansandu::farseer::internal::windows::operation
{

constexpr auto maximumReceiveBufferSize = 4096;

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

    virtual void scheduleAcceptSystemOperation(const SocketIdentifier listeningSocketIdentifier) = 0;

    virtual void scheduleReceiveSystemOperation(const SocketIdentifier socketIdentifier) = 0;

    virtual void
    scheduleSendBytesSystemOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) = 0;
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

    virtual dansandu::journey::Level getLoggingLevelFromErrorCode(const DWORD errorCode) const
    {
        if (errorCode == ERROR_NETNAME_DELETED || errorCode == ERROR_CONNECTION_ABORTED ||
            errorCode == ERROR_OPERATION_ABORTED)
        {
            return dansandu::journey::Level::info;
        }
        return dansandu::journey::Level::error;
    }

    // Always called on the operation consumer thread. Should be safe to access consumer thread resources without locks
    // inside this method. If the method returns true, then the operation can be discarded. Otherwise, the operation
    // was rescheduled and will be kept alive for another execution.
    virtual bool execute(
        IOperationScheduler& operationScheduler,
        dansandu::farseer::internal::windows::socket_container::SocketContainer& socketContainer,
        const DWORD numberOfBytesTransferred
    ) = 0;

    // Failure hook always called on the operation consumer thread when the operation failed to complete or to execute.
    // The operation is discarded afterwards, so any follow-up work must be scheduled as a new operation. The error code
    // is ERROR_SUCCESS if the operation completed but failed to execute. By default, the socket of the operation is
    // erased.
    virtual void onFailure(
        IOperationScheduler& operationScheduler,
        dansandu::farseer::internal::windows::socket_container::SocketContainer& socketContainer, const DWORD errorCode
    )
    {
        socketContainer.eraseSocket(getSocketIdentifier());
    }

    void eraseWsaOverlapped()
    {
        SecureZeroMemory(static_cast<WSAOVERLAPPED*>(this), sizeof(WSAOVERLAPPED));
    }
};

class IUserOperation : public IOperation
{
};

class ISystemOperation : public IOperation
{
public:
    // These operations are not scheduled directly by farseer users. Instead, these operations are spawned by other
    // operations through their IOperation::execute implementation. It should be safe to access consumer thread
    // resources without locks inside this method.
    virtual void schedule(
        IOperationScheduler& operationScheduler,
        dansandu::farseer::internal::windows::socket_container::SocketContainer& socketContainer
    ) = 0;
};

}
