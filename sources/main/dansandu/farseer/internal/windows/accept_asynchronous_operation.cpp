#include "dansandu/farseer/internal/windows/accept_asynchronous_operation.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::asynchronous_operation::Socket;

namespace dansandu::farseer::internal::windows::accept_asynchronous_operation
{

constexpr auto maximumReceiveBufferSize = 4096;

class AcceptAsynchronousOperation : public AsynchronousOperation
{
public:
    AcceptAsynchronousOperation(const SocketIdentifier pendingAcceptSocketIdentifier,
                                const SocketIdentifier listeningSocketIdentifier)
        : AsynchronousOperation{pendingAcceptSocketIdentifier}, listeningSocketIdentifier_{listeningSocketIdentifier}
    {
    }

    void postToCompletionPort(IAsynchronousOperationsScheduler& asynchronousOperationsScheduler) override
    {
        auto& listeningSocket = asynchronousOperationsScheduler.getSocketOrThrow(listeningSocketIdentifier_);

        const auto completionPort = asynchronousOperationsScheduler.getCompletionPort();

        asynchronousOperationsScheduler.insertSocket(
            socketIdentifier_,
            Socket{
                .socket = listeningSocket.socket.postAccept(receiveBuffer_, std::size(receiveBuffer_),
                                                            socketIdentifier_, completionPort, &overlapped_),
                .protocolReader = ProtocolReader{[&scheduler = asynchronousOperationsScheduler](
                                                     const SocketIdentifier receivingSocketIdentifier,
                                                     std::vector<uint8_t>&& response)
                                                 {
                                                     scheduler.createSendBytesAsynchronousOperation(
                                                         receivingSocketIdentifier, std::move(response));
                                                 }},
                .listeningSocketIdentifier = listeningSocketIdentifier_,
            });
    }

    bool finalize(IAsynchronousOperationsScheduler& asynchronousOperationsScheduler,
                  const DWORD numberOfBytesTransferred) override
    {
        auto& socket = asynchronousOperationsScheduler.getSocketOrThrow(socketIdentifier_);

        auto& listeningSocket = asynchronousOperationsScheduler.getSocketOrThrow(listeningSocketIdentifier_);

        socket.socket.accept(listeningSocket.socket);

        asynchronousOperationsScheduler.createAcceptAsynchronousOperation(listeningSocketIdentifier_);

        asynchronousOperationsScheduler.createReceiveAsynchronousOperation(socketIdentifier_);

        listeningSocket.connectionCallback(SocketEvent::clientOpen, socketIdentifier_);

        LOG_INFO("Accepted client socket with address ", socket.socket.getIpAddress(), ':', socket.socket.getPort());

        return true;
    }

    const char* getName() const override
    {
        return "AcceptAsynchronousOperation";
    }

private:
    const SocketIdentifier listeningSocketIdentifier_;
    char receiveBuffer_[maximumReceiveBufferSize];
};

std::unique_ptr<AsynchronousOperation>
createAcceptAsynchronousOperation(const SocketIdentifier pendingAcceptSocketIdentifier,
                                  const SocketIdentifier listeningSocketIdentifier)
{
    return std::make_unique<AcceptAsynchronousOperation>(pendingAcceptSocketIdentifier, listeningSocketIdentifier);
}

}
