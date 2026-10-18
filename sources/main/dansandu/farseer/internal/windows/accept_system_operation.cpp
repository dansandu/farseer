#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/accept_system_operation.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/internal/receive_buffer.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::receive_buffer::maximumReceiveBufferSize;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::ISystemOperation;
using dansandu::farseer::internal::windows::socket_container::Socket;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::farseer::internal::windows::windows_socket::WindowsSocket;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::accept_system_operation
{

namespace
{

// Stops the listening socket from spinning when AcceptEx keeps failing for reasons unrelated to the clients.
constexpr auto maximumConsecutiveAcceptFailures = std::size_t{16};

bool isClientFailure(const DWORD errorCode)
{
    return errorCode == ERROR_NETNAME_DELETED || errorCode == ERROR_CONNECTION_ABORTED || errorCode == WSAECONNRESET;
}

}

class AcceptSystemOperation : public ISystemOperation
{
public:
    AcceptSystemOperation(
        const SocketIdentifier listeningSocketIdentifier, const SocketIdentifier pendingAcceptSocketIdentifier
    )
        : listeningSocketIdentifier_{listeningSocketIdentifier},
          pendingAcceptSocketIdentifier_{pendingAcceptSocketIdentifier},
          acceptRescheduled_{false}
    {
    }

    const char* getName() const noexcept override
    {
        return "AcceptSystemOperation";
    }

    SocketIdentifier getSocketIdentifier() const noexcept override
    {
        return pendingAcceptSocketIdentifier_;
    }

    void schedule(IOperationScheduler& operationScheduler, SocketContainer& socketContainer) override
    {
        auto& listeningSocket = socketContainer.getSocketOrThrow(listeningSocketIdentifier_);

        const auto completionPort = operationScheduler.getCompletionPort();

        auto& socket = socketContainer.insertSocket(
            pendingAcceptSocketIdentifier_, Socket{
                                                .socket = WindowsSocket{},
                                                .protocolReader = ProtocolReader{},
                                                .listeningSocketIdentifier = listeningSocketIdentifier_,
                                                .inboundBytes = {},
                                                .outboundBytes = {},
                                                .pendingAccept = true,
                                                .consecutiveAcceptFailures = 0,
                                            }
        );

        SCOPE_FAILURE([&]() { socketContainer.eraseSocket(pendingAcceptSocketIdentifier_); });

        socket.socket = listeningSocket.socket.postAccept(
            receiveBuffer_, std::size(receiveBuffer_), pendingAcceptSocketIdentifier_, completionPort, this
        );
    }

    bool execute(
        IOperationScheduler& operationScheduler, SocketContainer& socketContainer, const DWORD numberOfBytesTransferred
    ) override
    {
        auto& acceptedSocket = socketContainer.getSocketOrThrow(pendingAcceptSocketIdentifier_);

        auto& listeningSocket = socketContainer.getSocketOrThrow(listeningSocketIdentifier_);

        acceptedSocket.socket.accept(listeningSocket.socket);

        listeningSocket.consecutiveAcceptFailures = 0;

        operationScheduler.scheduleAcceptSystemOperation(listeningSocketIdentifier_);

        acceptRescheduled_ = true;

        operationScheduler.scheduleReceiveSystemOperation(pendingAcceptSocketIdentifier_);

        acceptedSocket.pendingAccept = false;

        listeningSocket.connectionCallback(SocketEvent::clientOpen, pendingAcceptSocketIdentifier_);

        LOG_INFO(
            "Accepted client socket with ID ", pendingAcceptSocketIdentifier_, " and address ",
            acceptedSocket.socket.getIpAddress(), ":", acceptedSocket.socket.getPort()
        );

        return true;
    }

    void
    onFailure(IOperationScheduler& operationScheduler, SocketContainer& socketContainer, const DWORD errorCode) override
    {
        // A failed accept, such as a client resetting the connection before it was accepted, only affects the pending
        // client socket. The listening socket keeps accepting connections unless it was erased in the meantime.
        socketContainer.eraseSocket(pendingAcceptSocketIdentifier_);

        if (acceptRescheduled_ || !socketContainer.containsSocket(listeningSocketIdentifier_))
        {
            return;
        }

        // Client failures are not counted because they can only repeat as fast as clients connect. Counting them would
        // allow clients that keep resetting their connections to close the listening socket.
        if (!isClientFailure(errorCode))
        {
            auto& listeningSocket = socketContainer.getSocketOrThrow(listeningSocketIdentifier_);

            ++listeningSocket.consecutiveAcceptFailures;

            if (listeningSocket.consecutiveAcceptFailures >= maximumConsecutiveAcceptFailures)
            {
                LOG_ERROR(
                    "Accepting connections on listening socket with ID ", listeningSocketIdentifier_, " failed ",
                    listeningSocket.consecutiveAcceptFailures, " consecutive times and the socket will be erased"
                );

                socketContainer.eraseSocket(listeningSocketIdentifier_);

                return;
            }
        }

        // If the listening socket cannot post another accept, it can no longer accept connections, so it's erased to
        // notify the user.
        SCOPE_FAILURE([&]() { socketContainer.eraseSocket(listeningSocketIdentifier_); });

        operationScheduler.scheduleAcceptSystemOperation(listeningSocketIdentifier_);
    }

private:
    const SocketIdentifier listeningSocketIdentifier_;
    const SocketIdentifier pendingAcceptSocketIdentifier_;
    bool acceptRescheduled_;
    char receiveBuffer_[maximumReceiveBufferSize];
};

std::unique_ptr<ISystemOperation> createAcceptSystemOperation(
    const SocketIdentifier listeningSocketIdentifier, const SocketIdentifier pendingAcceptSocketIdentifier
)
{
    return std::make_unique<AcceptSystemOperation>(listeningSocketIdentifier, pendingAcceptSocketIdentifier);
}

}
#endif
