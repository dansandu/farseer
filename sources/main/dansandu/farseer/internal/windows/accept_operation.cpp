#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/accept_operation.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::windows::i_operation_scheduler::IOperationScheduler;
using dansandu::farseer::internal::windows::i_operation_scheduler::Operation;
using dansandu::farseer::internal::windows::i_operation_scheduler::Socket;

namespace dansandu::farseer::internal::windows::accept_operation
{

constexpr auto maximumReceiveBufferSize = 4096;

class AcceptOperation : public Operation
{
public:
    AcceptOperation(const SocketIdentifier pendingAcceptSocketIdentifier,
                    const SocketIdentifier listeningSocketIdentifier)
        : Operation{pendingAcceptSocketIdentifier}, listeningSocketIdentifier_{listeningSocketIdentifier}
    {
    }

    const char* getName() const override
    {
        return "AcceptOperation";
    }

    bool discard(const DWORD numberOfBytesTransferred) const override
    {
        return true;
    }

    void postToCompletionPort(IOperationScheduler& operationScheduler) override
    {
        auto& listeningSocket = operationScheduler.getSocketOrThrow(listeningSocketIdentifier_);

        const auto completionPort = operationScheduler.getCompletionPort();

        operationScheduler.insertSocket(
            socketIdentifier_,
            Socket{
                .socket = listeningSocket.socket.postAccept(receiveBuffer_, std::size(receiveBuffer_),
                                                            socketIdentifier_, completionPort, &overlapped_),
                .protocolReader = ProtocolReader{[&](const SocketIdentifier receivingSocketIdentifier,
                                                     std::vector<uint8_t>&& response)
                                                 {
                                                     operationScheduler.scheduleSendBytesOperation(
                                                         receivingSocketIdentifier, std::move(response));
                                                 }},
                .listeningSocketIdentifier = listeningSocketIdentifier_,
            });
    }

    void execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
    {
        auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

        auto& listeningSocket = operationScheduler.getSocketOrThrow(listeningSocketIdentifier_);

        socket.socket.accept(listeningSocket.socket);

        operationScheduler.scheduleAcceptOperation(listeningSocketIdentifier_);

        operationScheduler.scheduleReceiveOperation(socketIdentifier_);

        listeningSocket.connectionCallback(SocketEvent::clientOpen, socketIdentifier_);

        LOG_INFO("Accepted client socket with address ", socket.socket.getIpAddress(), ':', socket.socket.getPort());
    }

private:
    const SocketIdentifier listeningSocketIdentifier_;
    char receiveBuffer_[maximumReceiveBufferSize];
};

std::unique_ptr<Operation> createAcceptOperation(const SocketIdentifier pendingAcceptSocketIdentifier,
                                                 const SocketIdentifier listeningSocketIdentifier)
{
    return std::make_unique<AcceptOperation>(pendingAcceptSocketIdentifier, listeningSocketIdentifier);
}

}
#endif
