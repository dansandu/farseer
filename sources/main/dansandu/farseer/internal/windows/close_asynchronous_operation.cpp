#include "dansandu/farseer/internal/windows/close_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsFactory;
using dansandu::farseer::internal::windows::asynchronous_operation::initialCompletionKey;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::socket_service::closeSocketService;
using dansandu::farseer::internal::windows::socket_service::SocketServiceContainer;

namespace dansandu::farseer::internal::windows::close_asynchronous_operation
{

class CloseAsynchronousOperation : public AsynchronousOperation
{
public:
    explicit CloseAsynchronousOperation(const SocketServiceId serviceId) : AsynchronousOperation{serviceId}
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
        closeSocketService(socketServiceContainer, serviceId_);

        return true;
    }

    const char* getName() const override
    {
        return "CloseAsynchronousOperation";
    }
};

std::unique_ptr<AsynchronousOperation> createCloseAsynchronousOperation(const SocketServiceId serviceId)
{
    return std::make_unique<CloseAsynchronousOperation>(serviceId);
}

}
