#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/connect_user_operation.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::IUserOperation;
using dansandu::farseer::internal::windows::socket_container::Socket;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::farseer::internal::windows::windows_socket::WindowsSocket;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::connect_user_operation
{

class ConnectUserOperation : public IUserOperation
{
public:
    ConnectUserOperation(
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

    const char* getName() const noexcept override
    {
        return "ConnectUserOperation";
    }

    SocketIdentifier getSocketIdentifier() const noexcept override
    {
        return socketIdentifier_;
    }

    bool execute(
        IOperationScheduler& operationScheduler, SocketContainer& socketContainer, const DWORD numberOfBytesTransferred
    ) override
    {
        if (!connectionPending_)
        {
            const auto completionPort = operationScheduler.getCompletionPort();

            auto& socket = socketContainer.insertSocket(
                socketIdentifier_, Socket{
                                       .socket = WindowsSocket{completionPort, socketIdentifier_},
                                       .protocolReader = ProtocolReader{},
                                       .listeningSocketIdentifier = invalidSocketIdentifier,
                                       .connectionCallback = std::move(connectionCallback_),
                                       .inboundBytes = {},
                                       .outboundBytes = {},
                                       .pendingAccept = false,
                                       .consecutiveAcceptFailures = 0,
                                   }
            );

            SCOPE_FAILURE([&]() { socketContainer.eraseSocket(socketIdentifier_); });

            eraseWsaOverlapped();

            socket.socket.postConnect(ipAddress_, port_, this);

            connectionPending_ = true;

            return false;
        }
        else
        {
            auto& socket = socketContainer.getSocketOrThrow(socketIdentifier_);

            socket.socket.connect();

            socket.connectionCallback(SocketEvent::clientOpen, socketIdentifier_);

            operationScheduler.scheduleReceiveSystemOperation(socketIdentifier_);

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

std::unique_ptr<IUserOperation> createConnectUserOperation(
    const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
)
{
    return std::make_unique<ConnectUserOperation>(socketIdentifier, ipAddress, port, std::move(connectionCallback));
}

}
#endif
