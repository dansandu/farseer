#include "dansandu/farseer/internal/windows/listen_asynchronous_operation.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/windows_socket.hpp"

using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::asynchronous_operation::initialCompletionKey;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::socket_service::SocketService;
using dansandu::farseer::internal::windows::socket_service::SocketServiceContainer;
using dansandu::farseer::internal::windows::windows_socket::WindowsSocket;

namespace dansandu::farseer::internal::windows::listen_asynchronous_operation
{

class ListenAsynchronousOperation : public AsynchronousOperation
{
public:
    ListenAsynchronousOperation(Sequencer<SocketServiceId>& sequencer, const std::wstring& ipAddress, const int port,
                                ConnectionCallbackType&& connectionCallback)
        : AsynchronousOperation{sequencer.generate()},
          ipAddress_{ipAddress},
          port_{port},
          connectionCallback_{std::move(connectionCallback)}
    {
    }

    void postToCompletionPort(SocketServiceContainer& services,
                              IAsynchronousOperationsScheduler& asynchronousOperationsScheduler,
                              const HANDLE completionPort) override
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
                  IAsynchronousOperationsScheduler& asynchronousOperationsScheduler, const HANDLE completionPort,
                  const DWORD numberOfBytesTransferred) override
    {
        auto socket = WindowsSocket{completionPort, serviceId_};

        socket.listen(ipAddress_, port_);

        const auto [servicePosition, serviceInserted] = socketServiceContainer.insert(
            {serviceId_, SocketService{
                             .socket = std::move(socket),
                             .protocolReader = ProtocolReader{[&registry = asynchronousOperationsScheduler](
                                                                  const SocketServiceId receiverSocketServiceId,
                                                                  std::vector<uint8_t>&& response)
                                                              {
                                                                  registry.createSendBytesAsynchronousOperation(
                                                                      receiverSocketServiceId, std::move(response));
                                                              }},
                             .listeningServiceId = InvalidServiceId,
                             .connectionCallback = std::move(connectionCallback_),
                         }});

        if (!serviceInserted)
        {
            THROW(std::logic_error, "Couldn't open listening service with ID ", serviceId_.getUnderlying(),
                  " because the ID is used by another service");
        }

        SCOPE_FAILURE([&]() { socketServiceContainer.erase(servicePosition); });

        asynchronousOperationsScheduler.createAcceptAsynchronousOperation(serviceId_);

        servicePosition->second.connectionCallback(SocketServiceEvent::serverOpen, serviceId_);

        LOG_INFO("Opened listening socket with ID ", serviceId_.getUnderlying(), " and address ",
                 servicePosition->second.socket.getIpAddress(), ':', servicePosition->second.socket.getPort());

        return true;
    }

    const char* getName() const override
    {
        return "ListenAsynchronousOperation";
    }

private:
    const std::wstring ipAddress_;
    const int port_;
    ConnectionCallbackType connectionCallback_;
};

std::unique_ptr<AsynchronousOperation> createListenAsynchronousOperation(Sequencer<SocketServiceId>& sequencer,
                                                                         const std::wstring& ipAddress, const int port,
                                                                         ConnectionCallbackType&& connectionCallback)
{
    return std::make_unique<ListenAsynchronousOperation>(sequencer, ipAddress, port, std::move(connectionCallback));
}

}
