#include "dansandu/farseer/internal/windows/send_request_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;

namespace dansandu::farseer::internal::windows::send_request_asynchronous_operation
{

class SendRequestAsynchronousOperation : public AsynchronousOperation
{
public:
    SendRequestAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                     const ProtocolSequenceNumber protocolSequenceNumber, std::vector<uint8_t>&& bytes,
                                     UniqueFunction<void(std::any&&)>&& expectedResponseConsumer)
        : AsynchronousOperation{socketIdentifier},
          protocolSequenceNumber_{protocolSequenceNumber},
          bytes_{std::move(bytes)},
          expectedResponseConsumer_{std::move(expectedResponseConsumer)},
          sendBytesPending_{false}
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

            socket.protocolReader.registerOneShotExpectedResponseConsumer(protocolSequenceNumber_,
                                                                          std::move(expectedResponseConsumer_));

            SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));

            socket.socket.postSend(reinterpret_cast<CHAR*>(bytes_.data()), static_cast<ULONG>(bytes_.size()),
                                   &overlapped_);

            return false;
        }
        else
        {
            LOG_INFO("Sent request bytes using socket ID ", socketIdentifier_.getUnderlying());

            return true;
        }
    }

    const char* getName() const override
    {
        return "SendRequestAsynchronousOperation";
    }

private:
    ProtocolSequenceNumber protocolSequenceNumber_;
    std::vector<uint8_t> bytes_;
    UniqueFunction<void(std::any&&)> expectedResponseConsumer_;
    bool sendBytesPending_;
};

std::unique_ptr<AsynchronousOperation> createSendRequestAsynchronousOperation(
    const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber protocolSequenceNumber,
    std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& expectedResponseConsumer)
{
    return std::make_unique<SendRequestAsynchronousOperation>(socketIdentifier, protocolSequenceNumber,
                                                              std::move(bytes), std::move(expectedResponseConsumer));
}

}
