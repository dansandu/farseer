#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/send_request_user_operation.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

#include <vector>

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
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
        const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
        std::vector<uint8_t>&& requestBytes, OneShotResponseConsumer&& responseConsumer
    )
        : socketIdentifier_{socketIdentifier},
          sequenceNumber_{sequenceNumber},
          requestBytes_{std::move(requestBytes)},
          responseConsumer_{std::move(responseConsumer)},
          sendBytesPending_{false}
    {
    }

    const char* getName() const noexcept override
    {
        return "SendRequestUserOperation";
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

            socket.responseConsumerContainer.insert(sequenceNumber_, std::move(responseConsumer_));

            SCOPE_FAILURE([&]() { socket.responseConsumerContainer.erase(sequenceNumber_); });

            eraseWsaOverlapped();

            socket.socket.postSend(
                reinterpret_cast<CHAR*>(requestBytes_.data()), static_cast<ULONG>(requestBytes_.size()), this
            );

            return true;
        }
        else
        {
            LOG_INFO("Sent request bytes using socket ID ", socketIdentifier_);

            return false;
        }
    }

private:
    const SocketIdentifier socketIdentifier_;
    const ProtocolSequenceNumber sequenceNumber_;
    std::vector<uint8_t> requestBytes_;
    OneShotResponseConsumer responseConsumer_;
    bool sendBytesPending_;
};

std::unique_ptr<IUserOperation> createSendRequestUserOperation(
    const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
    std::vector<uint8_t>&& requestBytes, OneShotResponseConsumer&& responseConsumer
)
{
    return std::make_unique<SendRequestUserOperation>(
        socketIdentifier, sequenceNumber, std::move(requestBytes), std::move(responseConsumer)
    );
}

}
#endif
