#include "dansandu/farseer/internal/windows/send_request_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::asynchronous_operation::initialCompletionKey;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::socket_service::SocketServiceContainer;

namespace dansandu::farseer::internal::windows::send_request_asynchronous_operation
{

class SendRequestAsynchronousOperation : public AsynchronousOperation
{
public:
    SendRequestAsynchronousOperation(const SocketServiceId serviceId, const ProtocolSequenceNumber sequenceNumber,
                                     std::vector<uint8_t>&& bytes,
                                     UniqueFunction<void(std::any&&)>&& expectedResponseConsumer)
        : AsynchronousOperation{serviceId},
          sequenceNumber_{sequenceNumber},
          bytes_{std::move(bytes)},
          expectedResponseConsumer_{std::move(expectedResponseConsumer)},
          sendBytesPending_{false}
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

            servicePosition->second.protocolReader.registerOneShotExpectedResponseConsumer(
                sequenceNumber_, std::move(expectedResponseConsumer_));

            SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));

            servicePosition->second.socket.postSend(reinterpret_cast<CHAR*>(bytes_.data()),
                                                    static_cast<ULONG>(bytes_.size()), &overlapped_);

            return false;
        }
        else
        {
            LOG_INFO("Sent request bytes using service ID ", serviceId_.getUnderlying());

            return true;
        }
    }

    const char* getName() const override
    {
        return "SendRequestAsynchronousOperation";
    }

private:
    ProtocolSequenceNumber sequenceNumber_;
    std::vector<uint8_t> bytes_;
    UniqueFunction<void(std::any&&)> expectedResponseConsumer_;
    bool sendBytesPending_;
};

std::unique_ptr<AsynchronousOperation>
createSendRequestAsynchronousOperation(const SocketServiceId serviceId, const ProtocolSequenceNumber sequenceNumber,
                                       std::vector<uint8_t>&& bytes,
                                       UniqueFunction<void(std::any&&)>&& expectedResponseConsumer)
{
    return std::make_unique<SendRequestAsynchronousOperation>(serviceId, sequenceNumber, std::move(bytes),
                                                              std::move(expectedResponseConsumer));
}

}
