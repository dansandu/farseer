#include "dansandu/farseer/internal/windows/connect_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::asynchronous_operation::Socket;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::windows_socket::WindowsSocket;

namespace dansandu::farseer::internal::windows::connect_asynchronous_operation
{

class ConnectAsynchronousOperation : public AsynchronousOperation
{
public:
    ConnectAsynchronousOperation(const SocketIdentifier socketIdentifier, const std::wstring& ipAddress, const int port,
                                 ConnectionCallback&& connectionCallback)
        : AsynchronousOperation{socketIdentifier},
          ipAddress_{ipAddress},
          port_{port},
          connectionCallback_{std::move(connectionCallback)},
          connectionPending_{false}
    {
    }

    void postToCompletionPort(IAsynchronousOperationsScheduler& asynchronousOperationsScheduler) override
    {
        const auto completionPort = asynchronousOperationsScheduler.getCompletionPort();

        const auto numberOfBytesTransferred = 0;
        const auto postResult =
            ::PostQueuedCompletionStatus(completionPort, numberOfBytesTransferred, defaultCompletionKey, &overlapped_);

        if (!postResult)
        {
            THROW(std::runtime_error, "Posting ", getName(), " failed with error ", getLastErrorMessage());
        }
    }

    bool finalize(IAsynchronousOperationsScheduler& asynchronousOperationsScheduler,
                  const DWORD numberOfBytesTransferred) override
    {
        if (!connectionPending_)
        {
            const auto completionPort = asynchronousOperationsScheduler.getCompletionPort();

            auto tempSocket = WindowsSocket{completionPort, socketIdentifier_};

            SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));

            tempSocket.postConnect(ipAddress_, port_, &overlapped_);

            asynchronousOperationsScheduler.insertSocket(
                socketIdentifier_,
                Socket{
                    .socket = std::move(tempSocket),
                    .protocolReader = ProtocolReader{[&scheduler = asynchronousOperationsScheduler](
                                                         const SocketIdentifier receivingSocketIdentifier,
                                                         std::vector<uint8_t>&& response)
                                                     {
                                                         scheduler.createSendBytesAsynchronousOperation(
                                                             receivingSocketIdentifier, std::move(response));
                                                     }},
                    .listeningSocketIdentifier = invalidSocketIdentifier,
                    .connectionCallback = std::move(connectionCallback_),
                });

            connectionPending_ = true;

            return false;
        }
        else
        {
            auto& socket = asynchronousOperationsScheduler.getSocketOrThrow(socketIdentifier_);

            socket.socket.connect();

            socket.connectionCallback(SocketEvent::clientOpen, socketIdentifier_);

            asynchronousOperationsScheduler.createReceiveAsynchronousOperation(socketIdentifier_);

            LOG_INFO("Connected to socket with address ", socket.socket.getIpAddress(), ':', socket.socket.getPort());

            return true;
        }
    }

    const char* getName() const override
    {
        return "ConnectAsynchronousOperation";
    }

private:
    const std::wstring ipAddress_;
    const int port_;
    ConnectionCallback connectionCallback_;
    bool connectionPending_;
};

std::unique_ptr<AsynchronousOperation> createConnectAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                                                          const std::wstring& ipAddress, const int port,
                                                                          ConnectionCallback&& connectionCallback)
{
    return std::make_unique<ConnectAsynchronousOperation>(socketIdentifier, ipAddress, port,
                                                          std::move(connectionCallback));
}

}
