#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/accept_system_operation.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::ISystemOperation;
using dansandu::farseer::internal::windows::operation::maximumReceiveBufferSize;
using dansandu::farseer::internal::windows::socket_container::Socket;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::farseer::internal::windows::windows_socket::WindowsSocket;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::accept_system_operation
{

class AcceptSystemOperation : public ISystemOperation
{
public:
    AcceptSystemOperation(
        const SocketIdentifier listeningSocketIdentifier, const SocketIdentifier pendingAcceptSocketIdentifier
    )
        : listeningSocketIdentifier_{listeningSocketIdentifier},
          pendingAcceptSocketIdentifier_{pendingAcceptSocketIdentifier}
    {
    }

    const char* getName() const override
    {
        return "AcceptSystemOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return listeningSocketIdentifier_;
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

        operationScheduler.scheduleAcceptSystemOperation(listeningSocketIdentifier_);

        operationScheduler.scheduleReceiveSystemOperation(pendingAcceptSocketIdentifier_);

        listeningSocket.connectionCallback(SocketEvent::clientOpen, pendingAcceptSocketIdentifier_);

        LOG_INFO(
            "Accepted client socket with ID ", pendingAcceptSocketIdentifier_, " and address ",
            acceptedSocket.socket.getIpAddress(), ":", acceptedSocket.socket.getPort()
        );

        return true;
    }

private:
    const SocketIdentifier listeningSocketIdentifier_;
    const SocketIdentifier pendingAcceptSocketIdentifier_;
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
