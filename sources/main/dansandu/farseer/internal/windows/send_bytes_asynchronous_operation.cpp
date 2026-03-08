#include "dansandu/farseer/internal/windows/send_bytes_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;

namespace dansandu::farseer::internal::windows::send_bytes_asynchronous_operation
{

class SendBytesAsynchronousOperation : public AsynchronousOperation
{
public:
    SendBytesAsynchronousOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes)
        : AsynchronousOperation{socketIdentifier}, bytes_{std::move(bytes)}, sendBytesPending_{false}
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
        if (!sendBytesPending_)
        {
            sendBytesPending_ = true;

            auto& socket = asynchronousOperationsScheduler.getSocketOrThrow(socketIdentifier_);

            SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));

            socket.socket.postSend(reinterpret_cast<CHAR*>(bytes_.data()), static_cast<ULONG>(bytes_.size()),
                                   &overlapped_);

            return false;
        }
        else
        {
            LOG_INFO("Sent bytes using service ID ", socketIdentifier_.getUnderlying());

            return true;
        }
    }

    const char* getName() const override
    {
        return "SendBytesAsynchronousOperation";
    }

private:
    std::vector<uint8_t> bytes_;
    bool sendBytesPending_;
};

std::unique_ptr<AsynchronousOperation> createSendBytesAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                                                            std::vector<uint8_t>&& bytes)
{
    return std::make_unique<SendBytesAsynchronousOperation>(socketIdentifier, std::move(bytes));
}

}
