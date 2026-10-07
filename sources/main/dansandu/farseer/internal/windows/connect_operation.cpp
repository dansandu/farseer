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
    ConnectOperation(
        const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
        UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
    )
        : socketIdentifier_{socketIdentifier},
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

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    Level getLoggingLevelFromErrorCode(const DWORD errorCode) const override
    {
        return Level::error;
    }

    bool execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
    {
        if (!connectionPending_)
        {
            const auto completionPort = operationScheduler.getCompletionPort();

            auto tempSocket = WindowsSocket{completionPort, socketIdentifier_};

            eraseWsaOverlapped();

            tempSocket.postConnect(ipAddress_, port_, this);

            operationScheduler.insertSocket(
                socketIdentifier_, Socket{
                                       .socket = std::move(tempSocket),
                                       .protocolReader = ProtocolReader{},
                                       .listeningSocketIdentifier = invalidSocketIdentifier,
                                       .connectionCallback = std::move(connectionCallback_),
                                       .inboundBytes = {},
                                       .outboundBytes = {},
                                   }
            );

            connectionPending_ = true;

            return false;
        }
        else
        {
            auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

            socket.socket.connect();

            socket.connectionCallback(SocketEvent::clientOpen, socketIdentifier_);

            operationScheduler.scheduleReceiveOperation(socketIdentifier_);

            LOG_INFO(
                "Connected to socket with ID ", socketIdentifier_, " and address ", socket.socket.getIpAddress(), ":",
                socket.socket.getPort()
            );

            return true;
        }
    }

private:
    const SocketIdentifier socketIdentifier_;
    const std::string ipAddress_;
    const int port_;
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)> connectionCallback_;
    bool connectionPending_;
};

std::unique_ptr<IOperation> createConnectOperation(
    const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
)
{
    return std::make_unique<ConnectOperation>(socketIdentifier, ipAddress, port, std::move(connectionCallback));
}

}
#endif
