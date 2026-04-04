#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/connect_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::i_operation_scheduler::defaultCompletionKey;
using dansandu::farseer::internal::windows::i_operation_scheduler::IOperationScheduler;
using dansandu::farseer::internal::windows::i_operation_scheduler::Operation;
using dansandu::farseer::internal::windows::i_operation_scheduler::Socket;
using dansandu::farseer::internal::windows::windows_socket::WindowsSocket;

namespace dansandu::farseer::internal::windows::connect_operation
{

class ConnectOperation : public Operation
{
public:
    ConnectOperation(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                     ConnectionCallback&& connectionCallback)
        : Operation{socketIdentifier},
          ipAddress_{ipAddress},
          port_{port},
          connectionCallback_{std::move(connectionCallback)},
          connectionPending_{false}
    {
    }

    const char* getName() const override
    {
        return "ConnectOperation";
    }

    bool discard(const DWORD numberOfBytesTransferred) const override
    {
        return connectionPending_;
    }

    void postToCompletionPort(IOperationScheduler& operationScheduler) override
    {
        const auto completionPort = operationScheduler.getCompletionPort();

        const auto numberOfBytesTransferred = 0;
        const auto postResult =
            ::PostQueuedCompletionStatus(completionPort, numberOfBytesTransferred, defaultCompletionKey, &overlapped_);

        if (!postResult)
        {
            THROW(std::runtime_error, "Posting ", getName(), " failed with error ", getLastErrorMessage());
        }
    }

    void execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
    {
        if (!connectionPending_)
        {
            const auto completionPort = operationScheduler.getCompletionPort();

            auto tempSocket = WindowsSocket{completionPort, socketIdentifier_};

            SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));

            tempSocket.postConnect(ipAddress_, port_, &overlapped_);

            operationScheduler.insertSocket(
                socketIdentifier_,
                Socket{
                    .socket = std::move(tempSocket),
                    .protocolReader = ProtocolReader{[&](const SocketIdentifier receivingSocketIdentifier,
                                                         std::vector<uint8_t>&& response) {
                        operationScheduler.scheduleSendBytesOperation(receivingSocketIdentifier, std::move(response));
                    }},
                    .listeningSocketIdentifier = invalidSocketIdentifier,
                    .connectionCallback = std::move(connectionCallback_),
                });

            connectionPending_ = true;
        }
        else
        {
            auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

            socket.socket.connect();

            socket.connectionCallback(SocketEvent::clientOpen, socketIdentifier_);

            operationScheduler.scheduleReceiveOperation(socketIdentifier_);

            LOG_INFO("Connected to socket with ID ", socketIdentifier_, " and address ", socket.socket.getIpAddress(),
                     ":", socket.socket.getPort());
        }
    }

private:
    const std::string ipAddress_;
    const int port_;
    ConnectionCallback connectionCallback_;
    bool connectionPending_;
};

std::unique_ptr<Operation> createConnectOperation(const SocketIdentifier socketIdentifier, const std::string& ipAddress,
                                                  const int port, ConnectionCallback&& connectionCallback)
{
    return std::make_unique<ConnectOperation>(socketIdentifier, ipAddress, port, std::move(connectionCallback));
}

}
#endif
