#include "dansandu/farseer/internal/windows/accept_asynchronous_operation.hpp"

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsFactory;
using dansandu::farseer::internal::windows::socket_service::SocketServiceContainer;

namespace dansandu::farseer::internal::windows::accept_asynchronous_operation
{

constexpr auto maximumReceiveBufferSize = 4096;

class AcceptAsynchronousOperation : public AsynchronousOperation
{
public:
    AcceptAsynchronousOperation(Sequencer<SocketServiceId>& sequencer, const SocketServiceId listeningServiceId)
        : AsynchronousOperation{sequencer.generate()}, listeningServiceId_{listeningServiceId}
    {
    }

    void postToCompletionPort(SocketServiceContainer& services, const HANDLE completionPort) override
    {
        const auto listeningServicePosition = getServiceOrThrow(services, listeningServiceId_);

        auto pendingAcceptSocket = listeningServicePosition->second.socket.postAccept(
            receiveBuffer_, std::size(receiveBuffer_), serviceId_, completionPort, &overlapped_);

        const auto [servicePosition, serviceInserted] = services.insert(
            {serviceId_,
             {.socket = std::move(pendingAcceptSocket), .listeningServiceId = listeningServicePosition->first}});

        if (!serviceInserted)
        {
            THROW(std::logic_error, "Couldn't open accepting service with ID ", serviceId_.getInteger(),
                  " because the ID is used by another service");
        }
    }

    bool finalize(Sequencer<SocketServiceId>& sequencer, SocketServiceContainer& services,
                  IAsynchronousOperationsFactory& asynchronousOperationsFactory, const HANDLE completionPort,
                  const DWORD numberOfBytesTransferred) override
    {
        const auto servicePosition = getServiceOrThrow(services, serviceId_);

        const auto listeningServicePosition = getServiceOrThrow(services, listeningServiceId_);

        auto& socket = servicePosition->second.socket;

        socket.accept(listeningServicePosition->second.socket);

        asynchronousOperationsFactory.createAcceptAsynchronousOperation(listeningServiceId_);

        asynchronousOperationsFactory.createReceiveAsynchronousOperation(serviceId_);

        listeningServicePosition->second.connectionCallback(SocketServiceEvent::clientOpen, listeningServiceId_,
                                                            serviceId_);

        LOG_INFO("Accepted client socket with address ", socket.getIpAddress(), ':', socket.getPort());

        return true;
    }

    const char* getName() const override
    {
        return "AcceptAsynchronousOperation";
    }

private:
    const SocketServiceId listeningServiceId_;
    char receiveBuffer_[maximumReceiveBufferSize];
};

std::unique_ptr<AsynchronousOperation> createAcceptAsynchronousOperation(Sequencer<SocketServiceId>& sequencer,
                                                                         const SocketServiceId listeningServiceId)
{
    return std::make_unique<AcceptAsynchronousOperation>(sequencer, listeningServiceId);
}

}
