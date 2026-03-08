#include "dansandu/farseer/internal/windows/listen_asynchronous_operation.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::asynchronous_operation::Socket;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::windows_socket::WindowsSocket;

namespace dansandu::farseer::internal::windows::listen_asynchronous_operation
{

class ListenAsynchronousOperation : public AsynchronousOperation
{
public:
    ListenAsynchronousOperation(const SocketIdentifier socketIdentifier, const std::wstring& ipAddress, const int port,
                                ConnectionCallback&& connectionCallback)
        : AsynchronousOperation{socketIdentifier},
          ipAddress_{ipAddress},
          port_{port},
          connectionCallback_{std::move(connectionCallback)}
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
        const auto completionPort = asynchronousOperationsScheduler.getCompletionPort();

        auto tempSocket = WindowsSocket{completionPort, socketIdentifier_};

        tempSocket.listen(ipAddress_, port_);

        auto& socket = asynchronousOperationsScheduler.insertSocket(
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

        SCOPE_FAILURE([&]() { asynchronousOperationsScheduler.eraseSocket(socketIdentifier_); });

        asynchronousOperationsScheduler.createAcceptAsynchronousOperation(socketIdentifier_);

        socket.connectionCallback(SocketEvent::serverOpen, socketIdentifier_);

        LOG_INFO("Opened listening socket with ID ", socketIdentifier_.getUnderlying(), " and address ",
                 socket.socket.getIpAddress(), ':', socket.socket.getPort());

        return true;
    }

    const char* getName() const override
    {
        return "ListenAsynchronousOperation";
    }

private:
    const std::wstring ipAddress_;
    const int port_;
    ConnectionCallback connectionCallback_;
};

std::unique_ptr<AsynchronousOperation> createListenAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                                                         const std::wstring& ipAddress, const int port,
                                                                         ConnectionCallback&& connectionCallback)
{
    return std::make_unique<ListenAsynchronousOperation>(socketIdentifier, ipAddress, port,
                                                         std::move(connectionCallback));
}

}
