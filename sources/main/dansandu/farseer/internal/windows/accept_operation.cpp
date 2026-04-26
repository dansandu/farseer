#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/accept_operation.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::windows::operation::IOperation;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::Socket;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::accept_operation
{

class AcceptOperation : public IOperation
{
public:
    AcceptOperation(const SocketIdentifier listeningSocketIdentifier,
                    const SocketIdentifier pendingAcceptSocketIdentifier)
        : listeningSocketIdentifier_{listeningSocketIdentifier},
          pendingAcceptSocketIdentifier_{pendingAcceptSocketIdentifier}
    {
        SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));
    }

    const char* getName() const override
    {
        return "AcceptOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return listeningSocketIdentifier_;
    }

    Level getSystemErrorCodeLevel(const DWORD errorCode) const override
    {
        return Level::error;
    }

    bool discard(const DWORD numberOfBytesTransferred) const override
    {
        return true;
    }

    LPWSAOVERLAPPED getOverlapped() override
    {
        return &overlapped_;
    }

    void postToCompletionPort(IOperationScheduler& operationScheduler) override
    {
        auto& listeningSocket = operationScheduler.getSocketOrThrow(listeningSocketIdentifier_);

        const auto completionPort = operationScheduler.getCompletionPort();

        operationScheduler.insertSocket(
            pendingAcceptSocketIdentifier_,
            Socket{
                .socket =
                    listeningSocket.socket.postAccept(receiveBuffer_, std::size(receiveBuffer_),
                                                      pendingAcceptSocketIdentifier_, completionPort, &overlapped_),
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
        auto& acceptedSocket = operationScheduler.getSocketOrThrow(pendingAcceptSocketIdentifier_);

        auto& listeningSocket = operationScheduler.getSocketOrThrow(listeningSocketIdentifier_);

        acceptedSocket.socket.accept(listeningSocket.socket);

        operationScheduler.scheduleAcceptOperation(listeningSocketIdentifier_);

        operationScheduler.scheduleReceiveOperation(pendingAcceptSocketIdentifier_);

        listeningSocket.connectionCallback(SocketEvent::clientOpen, pendingAcceptSocketIdentifier_);

        LOG_INFO("Accepted client socket with ID ", pendingAcceptSocketIdentifier_, " and address ",
                 acceptedSocket.socket.getIpAddress(), ":", acceptedSocket.socket.getPort());
    }

private:
    static constexpr auto maximumReceiveBufferSize = 4096;

    const SocketIdentifier listeningSocketIdentifier_;
    const SocketIdentifier pendingAcceptSocketIdentifier_;
    char receiveBuffer_[maximumReceiveBufferSize];
    WSAOVERLAPPED overlapped_;
};

std::unique_ptr<IOperation> createAcceptOperation(const SocketIdentifier listeningSocketIdentifier,
                                                  const SocketIdentifier pendingAcceptSocketIdentifier)
{
    return std::make_unique<AcceptOperation>(listeningSocketIdentifier, pendingAcceptSocketIdentifier);
}

}
#endif
