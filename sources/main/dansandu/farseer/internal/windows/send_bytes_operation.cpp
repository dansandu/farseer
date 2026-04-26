#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/send_bytes_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::operation::IOperation;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::send_bytes_operation
{

class SendBytesOperation : public IOperation
{
public:
    SendBytesOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes)
        : socketIdentifier_{socketIdentifier}, bytes_{std::move(bytes)}, sendBytesPending_{false}
    {
        SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));
    }

    const char* getName() const override
    {
        return "SendBytesOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    Level getSystemErrorCodeLevel(const DWORD errorCode) const override
    {
        return Level::error;
    }

    bool discard(const DWORD numberOfBytesTransferred) const override
    {
        return sendBytesPending_;
    }

    LPWSAOVERLAPPED getOverlapped() override
    {
        return &overlapped_;
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

            LOG_DEBUG("Sending ", bytes_.size(), " bytes to socket with ID ", socketIdentifier_);

            socket.socket.postSend(reinterpret_cast<CHAR*>(bytes_.data()), static_cast<ULONG>(bytes_.size()),
                                   &overlapped_);
        }
        else
        {
            LOG_INFO("Sent ", bytes_.size(), " bytes to socket with ID ", socketIdentifier_);
        }
    }

private:
    const SocketIdentifier socketIdentifier_;
    std::vector<uint8_t> bytes_;
    bool sendBytesPending_;
    WSAOVERLAPPED overlapped_;
};

std::unique_ptr<IOperation> createSendBytesOperation(const SocketIdentifier socketIdentifier,
                                                     std::vector<uint8_t>&& bytes)
{
    return std::make_unique<SendBytesOperation>(socketIdentifier, std::move(bytes));
}

}
#endif
