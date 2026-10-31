#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/send_bytes_user_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::IUserOperation;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::send_bytes_user_operation
{

class SendBytesUserOperation : public IUserOperation
{
public:
    SendBytesUserOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes)
        : socketIdentifier_{socketIdentifier}, bytes_{std::move(bytes)}, sendBytesPending_{false}
    {
    }

    const char* getName() const noexcept override
    {
        return "SendBytesUserOperation";
    }

    SocketIdentifier getSocketIdentifier() const noexcept override
    {
        return socketIdentifier_;
    }

    bool execute(
        IOperationScheduler& operationScheduler, SocketContainer& socketContainer, const DWORD numberOfBytesTransferred
    ) override
    {
        if (!sendBytesPending_)
        {
            sendBytesPending_ = true;

            auto& socket = socketContainer.getSocketOrThrow(socketIdentifier_);

            eraseWsaOverlapped();

            LOG_DEBUG("Sending ", bytes_.size(), " bytes to socket with ID ", socketIdentifier_);

            socket.socket.postSend(reinterpret_cast<CHAR*>(bytes_.data()), static_cast<ULONG>(bytes_.size()), this);

            return true;
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

            return false;
        }
    }

private:
    const SocketIdentifier socketIdentifier_;
    std::vector<uint8_t> bytes_;
    bool sendBytesPending_;
};

std::unique_ptr<IUserOperation>
createSendBytesUserOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes)
{
    return std::make_unique<SendBytesUserOperation>(socketIdentifier, std::move(bytes));
}

}
#endif
