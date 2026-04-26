#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/listen_operation.hpp"
#include "dansandu/ballotin/scope.hpp"
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

namespace dansandu::farseer::internal::windows::listen_operation
{

class ListenOperation : public IOperation
{
public:
    ListenOperation(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                    ConnectionCallback&& connectionCallback)
        : socketIdentifier_{socketIdentifier},
          ipAddress_{ipAddress},
          port_{port},
          connectionCallback_{std::move(connectionCallback)}
    {
        SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));
    }

    const char* getName() const override
    {
        return "ListenOperation";
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
        return true;
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
        const auto completionPort = operationScheduler.getCompletionPort();

        auto tempSocket = WindowsSocket{completionPort, socketIdentifier_};

        tempSocket.listen(ipAddress_, port_);

        auto& socket = operationScheduler.insertSocket(
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

        SCOPE_FAILURE([&]() { operationScheduler.eraseSocket(socketIdentifier_); });

        operationScheduler.scheduleAcceptOperation(socketIdentifier_);

        socket.connectionCallback(SocketEvent::serverOpen, socketIdentifier_);

        LOG_INFO("Opened listening socket with ID ", socketIdentifier_, " and address ", socket.socket.getIpAddress(),
                 ':', socket.socket.getPort());
    }

private:
    const SocketIdentifier socketIdentifier_;
    const std::string ipAddress_;
    const int port_;
    ConnectionCallback connectionCallback_;
    WSAOVERLAPPED overlapped_;
};

std::unique_ptr<IOperation> createListenOperation(const SocketIdentifier socketIdentifier, const std::string& ipAddress,
                                                  const int port, ConnectionCallback&& connectionCallback)
{
    return std::make_unique<ListenOperation>(socketIdentifier, ipAddress, port, std::move(connectionCallback));
}

}
#endif
