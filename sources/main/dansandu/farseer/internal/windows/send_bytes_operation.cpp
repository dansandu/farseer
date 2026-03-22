#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/send_bytes_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::i_operation_scheduler::defaultCompletionKey;
using dansandu::farseer::internal::windows::i_operation_scheduler::IOperationScheduler;
using dansandu::farseer::internal::windows::i_operation_scheduler::Operation;

namespace dansandu::farseer::internal::windows::send_bytes_operation
{

class SendBytesOperation : public Operation
{
public:
    SendBytesOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes)
        : Operation{socketIdentifier}, bytes_{std::move(bytes)}, sendBytesPending_{false}
    {
    }

    const char* getName() const override
    {
        return "SendBytesOperation";
    }

    bool discard(const DWORD numberOfBytesTransferred) const
    {
        return sendBytesPending_;
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

    void execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
    {
        if (!sendBytesPending_)
        {
            sendBytesPending_ = true;

            auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

            SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));

            socket.socket.postSend(reinterpret_cast<CHAR*>(bytes_.data()), static_cast<ULONG>(bytes_.size()),
                                   &overlapped_);
        }
        else
        {
            LOG_INFO("Sent bytes using socket ID ", socketIdentifier_.getUnderlying());
        }
    }

private:
    std::vector<uint8_t> bytes_;
    bool sendBytesPending_;
};

std::unique_ptr<Operation> createSendBytesOperation(const SocketIdentifier socketIdentifier,
                                                    std::vector<uint8_t>&& bytes)
{
    return std::make_unique<SendBytesOperation>(socketIdentifier, std::move(bytes));
}

}
#endif
