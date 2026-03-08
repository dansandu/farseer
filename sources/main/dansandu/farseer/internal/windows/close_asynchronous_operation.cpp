#include "dansandu/farseer/internal/windows/close_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;

namespace dansandu::farseer::internal::windows::close_asynchronous_operation
{

class CloseAsynchronousOperation : public AsynchronousOperation
{
public:
    explicit CloseAsynchronousOperation(const SocketIdentifier socketIdentifier)
        : AsynchronousOperation{socketIdentifier}
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
        asynchronousOperationsScheduler.eraseSocket(socketIdentifier_);

        return true;
    }

    const char* getName() const override
    {
        return "CloseAsynchronousOperation";
    }
};

std::unique_ptr<AsynchronousOperation> createCloseAsynchronousOperation(const SocketIdentifier socketIdentifier)
{
    return std::make_unique<CloseAsynchronousOperation>(socketIdentifier);
}

}
