#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/listen_user_operation.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::IUserOperation;
using dansandu::farseer::internal::windows::socket_container::Socket;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::farseer::internal::windows::windows_socket::WindowsSocket;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::listen_user_operation
{

class ListenUserOperation : public IUserOperation
{
public:
    ListenUserOperation(
        const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
        UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
    )
        : socketIdentifier_{socketIdentifier},
          ipAddress_{ipAddress},
          port_{port},
          connectionCallback_{std::move(connectionCallback)}
    {
    }

    const char* getName() const override
    {
        return "ListenUserOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    bool execute(
        IOperationScheduler& operationScheduler, SocketContainer& socketContainer, const DWORD numberOfBytesTransferred
    ) override
    {
        const auto completionPort = operationScheduler.getCompletionPort();

        auto tempSocket = WindowsSocket{completionPort, socketIdentifier_};

        tempSocket.listen(ipAddress_, port_);

        auto& socket = socketContainer.insertSocket(
            socketIdentifier_, Socket{
                                   .socket = std::move(tempSocket),
                                   .protocolReader = ProtocolReader{},
                                   .listeningSocketIdentifier = invalidSocketIdentifier,
                                   .connectionCallback = std::move(connectionCallback_),
                                   .inboundBytes = {},
                                   .outboundBytes = {},
                                   .pendingAccept = false,
                               }
        );

        SCOPE_FAILURE([&]() { socketContainer.eraseSocket(socketIdentifier_); });

        operationScheduler.scheduleAcceptSystemOperation(socketIdentifier_);

        socket.connectionCallback(SocketEvent::serverOpen, socketIdentifier_);

        LOG_INFO(
            "Opened listening socket with ID ", socketIdentifier_, " and address ", socket.socket.getIpAddress(), ':',
            socket.socket.getPort()
        );

        return true;
    }

private:
    const SocketIdentifier socketIdentifier_;
    const std::string ipAddress_;
    const int port_;
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)> connectionCallback_;
};

std::unique_ptr<IUserOperation> createListenUserOperation(
    const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback
)
{
    return std::make_unique<ListenUserOperation>(socketIdentifier, ipAddress, port, std::move(connectionCallback));
}

}
#endif
