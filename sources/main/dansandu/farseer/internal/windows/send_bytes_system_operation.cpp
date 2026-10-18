#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/send_bytes_system_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::ISystemOperation;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::send_bytes_system_operation
{

class SendBytesSystemOperation : public ISystemOperation
{
public:
    SendBytesSystemOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes)
        : socketIdentifier_{socketIdentifier}, bytes_{std::move(bytes)}
    {
    }

    const char* getName() const noexcept override
    {
        return "SendBytesSystemOperation";
    }

    SocketIdentifier getSocketIdentifier() const noexcept override
    {
        return socketIdentifier_;
    }

    void schedule(IOperationScheduler& operationScheduler, SocketContainer& socketContainer) override
    {
        auto& socket = socketContainer.getSocketOrThrow(socketIdentifier_);

        LOG_DEBUG("Sending ", bytes_.size(), " bytes to socket with ID ", socketIdentifier_);

        socket.socket.postSend(reinterpret_cast<CHAR*>(bytes_.data()), static_cast<ULONG>(bytes_.size()), this);
    }

    bool execute(
        IOperationScheduler& operationScheduler, SocketContainer& socketContainer, const DWORD numberOfBytesTransferred
    ) override
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

private:
    const SocketIdentifier socketIdentifier_;
    std::vector<uint8_t> bytes_;
};

std::unique_ptr<ISystemOperation>
createSendBytesSystemOperation(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes)
{
    return std::make_unique<SendBytesSystemOperation>(socketIdentifier, std::move(bytes));
}

}
#endif
