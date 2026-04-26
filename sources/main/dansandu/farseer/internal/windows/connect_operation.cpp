#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/connect_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::operation::IOperation;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::Socket;
using dansandu::farseer::internal::windows::windows_socket::WindowsSocket;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::connect_operation
{

class ConnectOperation : public IOperation
{
public:
    ConnectOperation(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                     ConnectionCallback&& connectionCallback)
        : socketIdentifier_{socketIdentifier},
          ipAddress_{ipAddress},
          port_{port},
          connectionCallback_{std::move(connectionCallback)},
          connectionPending_{false}
    {
        SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));
    }

    const char* getName() const override
    {
        return "ConnectOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    Level getSystemErrorCodeLevel(const DWORD errorCode) const override
    {
        return Level::error;
    }

    bool discard(const DWORD numberOfBytesTransferred) const override
    {
        return connectionPending_;
    }

    LPWSAOVERLAPPED getOverlapped() override
    {
        return &overlapped_;
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
                                                         std::vector<uint8_t>&& response)
                                                     {
                                                         operationScheduler.scheduleSendBytesOperation(
                                                             receivingSocketIdentifier, std::move(response));
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
    const SocketIdentifier socketIdentifier_;
    const std::string ipAddress_;
    const int port_;
    ConnectionCallback connectionCallback_;
    bool connectionPending_;
    WSAOVERLAPPED overlapped_;
};

std::unique_ptr<IOperation> createConnectOperation(const SocketIdentifier socketIdentifier,
                                                   const std::string& ipAddress, const int port,
                                                   ConnectionCallback&& connectionCallback)
{
    return std::make_unique<ConnectOperation>(socketIdentifier, ipAddress, port, std::move(connectionCallback));
}

}
#endif
