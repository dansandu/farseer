#include "dansandu/farseer/internal/windows/connect_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsFactory;
using dansandu::farseer::internal::windows::asynchronous_operation::initialCompletionKey;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::socket_service::SocketServiceContainer;
using dansandu::farseer::internal::windows::windows_socket::WindowsSocket;

namespace dansandu::farseer::internal::windows::connect_asynchronous_operation
{

class ConnectAsynchronousOperation : public AsynchronousOperation
{
public:
    ConnectAsynchronousOperation(Sequencer<SocketServiceId>& sequencer, const std::wstring& ipAddress, const int port,
                                 ConnectionCallbackType connectionCallback)
        : AsynchronousOperation{sequencer.generate()},
          ipAddress_{ipAddress},
          port_{port},
          connectionCallback_{std::move(connectionCallback)},
          connectionPending_{false}
    {
    }

    void postToCompletionPort(SocketServiceContainer& services, const HANDLE completionPort) override
    {
        const auto numberOfBytesTransferred = 0;
        const auto postResult =
            ::PostQueuedCompletionStatus(completionPort, numberOfBytesTransferred, initialCompletionKey, &overlapped_);

        if (!postResult)
        {
            THROW(std::runtime_error, "Posting ", getName(), " failed with error ", getLastErrorMessage());
        }
    }

    bool finalize(Sequencer<SocketServiceId>& sequencer, SocketServiceContainer& socketServiceContainer,
                  IAsynchronousOperationsFactory& asynchronousOperationsFactory, const HANDLE completionPort,
                  const DWORD numberOfBytesTransferred) override
    {
        if (!connectionPending_)
        {
            auto socket = WindowsSocket{completionPort, serviceId_};

            SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));

            socket.postConnect(ipAddress_, port_, &overlapped_);

            const auto [servicePosition, serviceInserted] =
                socketServiceContainer.insert({serviceId_,
                                               {
                                                   .socket = std::move(socket),
                                                   .listeningServiceId = InvalidServiceId,
                                                   .connectionCallback = std::move(connectionCallback_),
                                               }});

            if (!serviceInserted)
            {
                THROW(std::logic_error, "Couldn't open connection service with ID ", serviceId_.getInteger(),
                      " because the ID is used by another service");
            }

            connectionPending_ = true;

            return false;
        }
        else
        {
            const auto servicePosition = getServiceOrThrow(socketServiceContainer, serviceId_);

            auto& socket = servicePosition->second.socket;

            socket.connect();

            servicePosition->second.connectionCallback(SocketServiceEvent::clientOpen, InvalidServiceId, serviceId_);

            asynchronousOperationsFactory.createReceiveAsynchronousOperation(serviceId_);

            LOG_INFO("Connected to socket with address ", socket.getIpAddress(), ':', socket.getPort());

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
    ConnectionCallbackType connectionCallback_;
    bool connectionPending_;
};

std::unique_ptr<AsynchronousOperation> createConnectAsynchronousOperation(Sequencer<SocketServiceId>& sequencer,
                                                                          const std::wstring& ipAddress, const int port,
                                                                          ConnectionCallbackType connectionCallback)
{
    return std::make_unique<ConnectAsynchronousOperation>(sequencer, ipAddress, port, std::move(connectionCallback));
}

}
