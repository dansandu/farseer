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
    }

    const char* getName() const override
    {
        return "SendBytesOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    Level getLoggingLevelFromErrorCode(const DWORD errorCode) const override
    {
        return Level::error;
    }

    bool execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
    {
        if (!sendBytesPending_)
        {
            sendBytesPending_ = true;

            auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

            eraseWsaOverlapped();

            LOG_DEBUG("Sending ", bytes_.size(), " bytes to socket with ID ", socketIdentifier_);

            socket.socket.postSend(reinterpret_cast<CHAR*>(bytes_.data()), static_cast<ULONG>(bytes_.size()), this);

            return false;
        }
        else
        {
            if (numberOfBytesTransferred == static_cast<DWORD>(bytes_.size()))
            {
                LOG_INFO("Sent ", bytes_.size(), " bytes to socket with ID ", socketIdentifier_);
            }
            else
            {
                THROW(
                    std::logic_error, "The number of bytes sent ", numberOfBytesTransferred,
                    " does not match the buffer size ", bytes_.size()
                );
            }

            return true;
        }
    }

private:
    const SocketIdentifier socketIdentifier_;
    std::vector<uint8_t> bytes_;
    bool sendBytesPending_;
};

std::unique_ptr<IOperation>
createSendBytesOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes)
{
    return std::make_unique<SendBytesOperation>(socketIdentifier, std::move(bytes));
}

}
#endif
