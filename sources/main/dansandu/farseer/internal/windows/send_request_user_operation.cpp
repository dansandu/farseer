#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/send_request_user_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::IUserOperation;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::send_request_user_operation
{

class SendRequestUserOperation : public IUserOperation
{
public:
    SendRequestUserOperation(
        const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber protocolSequenceNumber,
        std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer
    )
        : socketIdentifier_{socketIdentifier},
          protocolSequenceNumber_{protocolSequenceNumber},
          bytes_{std::move(bytes)},
          responseConsumer_{std::move(responseConsumer)},
          sendBytesPending_{false}
    {
    }

    const char* getName() const override
    {
        return "SendRequestUserOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
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

            socket.protocolReader.registerInboundOneShotResponseConsumer(
                protocolSequenceNumber_, std::move(responseConsumer_)
            );

            eraseWsaOverlapped();

            socket.socket.postSend(reinterpret_cast<CHAR*>(bytes_.data()), static_cast<ULONG>(bytes_.size()), this);

            return false;
        }
        else
        {
            LOG_INFO("Sent request bytes using socket ID ", socketIdentifier_);

            return true;
        }
    }

private:
    const SocketIdentifier socketIdentifier_;
    const ProtocolSequenceNumber protocolSequenceNumber_;
    std::vector<uint8_t> bytes_;
    UniqueFunction<void(std::any&&)> responseConsumer_;
    bool sendBytesPending_;
};

std::unique_ptr<IUserOperation> createSendRequestUserOperation(
    const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber protocolSequenceNumber,
    std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer
)
{
    return std::make_unique<SendRequestUserOperation>(
        socketIdentifier, protocolSequenceNumber, std::move(bytes), std::move(responseConsumer)
    );
}

}
#endif
