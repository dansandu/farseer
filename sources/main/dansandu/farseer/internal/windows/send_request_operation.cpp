#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/send_request_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::i_operation_scheduler::defaultCompletionKey;
using dansandu::farseer::internal::windows::i_operation_scheduler::IOperationScheduler;
using dansandu::farseer::internal::windows::i_operation_scheduler::Operation;

namespace dansandu::farseer::internal::windows::send_request_operation
{

class SendRequestOperation : public Operation
{
public:
    SendRequestOperation(const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber protocolSequenceNumber,
                         std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer)
        : Operation{socketIdentifier},
          protocolSequenceNumber_{protocolSequenceNumber},
          bytes_{std::move(bytes)},
          responseConsumer_{std::move(responseConsumer)},
          sendBytesPending_{false}
    {
    }

    const char* getName() const override
    {
        return "SendRequestOperation";
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

            socket.protocolReader.registerOneShotResponseConsumer(protocolSequenceNumber_,
                                                                  std::move(responseConsumer_));

            SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));

            socket.socket.postSend(reinterpret_cast<CHAR*>(bytes_.data()), static_cast<ULONG>(bytes_.size()),
                                   &overlapped_);
        }
        else
        {
            LOG_INFO("Sent request bytes using socket ID ", socketIdentifier_.getUnderlying());
        }
    }

private:
    ProtocolSequenceNumber protocolSequenceNumber_;
    std::vector<uint8_t> bytes_;
    UniqueFunction<void(std::any&&)> responseConsumer_;
    bool sendBytesPending_;
};

std::unique_ptr<Operation> createSendRequestOperation(const SocketIdentifier socketIdentifier,
                                                      const ProtocolSequenceNumber protocolSequenceNumber,
                                                      std::vector<uint8_t>&& bytes,
                                                      UniqueFunction<void(std::any&&)>&& responseConsumer)
{
    return std::make_unique<SendRequestOperation>(socketIdentifier, protocolSequenceNumber, std::move(bytes),
                                                  std::move(responseConsumer));
}

}
#endif
