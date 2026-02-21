#include "dansandu/farseer/internal/windows/send_bytes_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::asynchronous_operation::initialCompletionKey;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::socket_service::SocketServiceContainer;

namespace dansandu::farseer::internal::windows::send_bytes_asynchronous_operation
{

class SendBytesAsynchronousOperation : public AsynchronousOperation
{
public:
    SendBytesAsynchronousOperation(const SocketServiceId serviceId, std::vector<uint8_t>&& bytes)
        : AsynchronousOperation{serviceId}, bytes_{std::move(bytes)}, sendBytesPending_{false}
    {
    }

    void postToCompletionPort(SocketServiceContainer& services,
                              IAsynchronousOperationsScheduler& asynchronousOperationsScheduler,
                              const HANDLE completionPort) override
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
                  IAsynchronousOperationsScheduler& asynchronousOperationsScheduler, const HANDLE completionPort,
                  const DWORD numberOfBytesTransferred) override
    {
        if (!sendBytesPending_)
        {
            sendBytesPending_ = true;

            const auto servicePosition = getServiceOrThrow(socketServiceContainer, serviceId_);

            SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));

            servicePosition->second.socket.postSend(reinterpret_cast<CHAR*>(bytes_.data()),
                                                    static_cast<ULONG>(bytes_.size()), &overlapped_);

            return false;
        }
        else
        {
            LOG_INFO("Sent bytes using service ID ", serviceId_.getUnderlying());

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

std::unique_ptr<AsynchronousOperation> createSendBytesAsynchronousOperation(const SocketServiceId serviceId,
                                                                            std::vector<uint8_t>&& bytes)
{
    return std::make_unique<SendBytesAsynchronousOperation>(serviceId, std::move(bytes));
}

}
