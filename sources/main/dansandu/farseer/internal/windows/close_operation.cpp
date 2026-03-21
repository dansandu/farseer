#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/close_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation_scheduler::defaultCompletionKey;
using dansandu::farseer::internal::windows::operation_scheduler::IOperationScheduler;
using dansandu::farseer::internal::windows::operation_scheduler::Operation;

namespace dansandu::farseer::internal::windows::close_operation
{

class CloseOperation : public Operation
{
public:
    explicit CloseOperation(const SocketIdentifier socketIdentifier) : Operation{socketIdentifier}
    {
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

    bool finalize(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
    {
        operationScheduler.eraseSocket(socketIdentifier_);

        return true;
    }

    const char* getName() const override
    {
        return "CloseOperation";
    }
};

std::unique_ptr<Operation> createCloseOperation(const SocketIdentifier socketIdentifier)
{
    return std::make_unique<CloseOperation>(socketIdentifier);
}

}
#endif
