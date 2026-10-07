#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/accept_operation.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::windows::operation::INonUserOperation;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::maximumReceiveBufferSize;
using dansandu::farseer::internal::windows::operation::Socket;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::accept_operation
{

class AcceptOperation : public INonUserOperation
{
public:
    AcceptOperation(
        const SocketIdentifier listeningSocketIdentifier, const SocketIdentifier pendingAcceptSocketIdentifier
    )
        : listeningSocketIdentifier_{listeningSocketIdentifier},
          pendingAcceptSocketIdentifier_{pendingAcceptSocketIdentifier}
    {
    }

    const char* getName() const override
    {
        return "AcceptOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return listeningSocketIdentifier_;
    }

    Level getLoggingLevelFromErrorCode(const DWORD errorCode) const override
    {
        return Level::error;
    }

    void schedule(IOperationScheduler& operationScheduler) override
    {
        auto& listeningSocket = operationScheduler.getSocketOrThrow(listeningSocketIdentifier_);

        const auto completionPort = operationScheduler.getCompletionPort();

        operationScheduler.insertSocket(
            pendingAcceptSocketIdentifier_,
            Socket{
                .socket = listeningSocket.socket.postAccept(
                    receiveBuffer_, std::size(receiveBuffer_), pendingAcceptSocketIdentifier_, completionPort, this
                ),
                .protocolReader = ProtocolReader{},
                .listeningSocketIdentifier = listeningSocketIdentifier_,
                .inboundBytes = {},
                .outboundBytes = {},
            }
        );
    }

    bool execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
    {
        auto& acceptedSocket = operationScheduler.getSocketOrThrow(pendingAcceptSocketIdentifier_);

        auto& listeningSocket = operationScheduler.getSocketOrThrow(listeningSocketIdentifier_);

        acceptedSocket.socket.accept(listeningSocket.socket);

        operationScheduler.scheduleAcceptOperation(listeningSocketIdentifier_);

        operationScheduler.scheduleReceiveOperation(pendingAcceptSocketIdentifier_);

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

std::unique_ptr<INonUserOperation> createAcceptOperation(
    const SocketIdentifier listeningSocketIdentifier, const SocketIdentifier pendingAcceptSocketIdentifier
)
{
    return std::make_unique<AcceptOperation>(listeningSocketIdentifier, pendingAcceptSocketIdentifier);
}

}
#endif
