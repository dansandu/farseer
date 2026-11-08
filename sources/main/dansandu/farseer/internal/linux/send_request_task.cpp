#if defined(__linux__)
#include "dansandu/farseer/internal/linux/send_request_task.hpp"
#include "dansandu/farseer/common.hpp"

using dansandu::farseer::internal::linux::socket_container::SocketContainer;
using dansandu::farseer::internal::linux::task::ITask;

namespace dansandu::farseer::internal::linux::send_request_task
{

class SendRequestTask : public ITask
{
public:
    SendRequestTask(
        const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
        std::vector<uint8_t>&& requestBytes, OneShotResponseConsumer&& responseConsumer
    )
        : socketIdentifier_{socketIdentifier},
          sequenceNumber_{sequenceNumber},
          requestBytes_{std::move(requestBytes)},
          responseConsumer_{std::move(responseConsumer)}
    {
    }

    const char* getName() const noexcept override
    {
        return "SendRequestTask";
    }

    SocketIdentifier getSocketIdentifier() const noexcept override
    {
        return socketIdentifier_;
    }

    void execute(SocketContainer& socketContainer) override
    {
        socketContainer.sendRequest(
            socketIdentifier_, sequenceNumber_, std::move(requestBytes_), std::move(responseConsumer_)
        );
    }

private:
    const SocketIdentifier socketIdentifier_;
    const ProtocolSequenceNumber sequenceNumber_;
    std::vector<uint8_t> requestBytes_;
    OneShotResponseConsumer responseConsumer_;
};

std::unique_ptr<ITask> createSendRequestTask(
    const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
    std::vector<uint8_t>&& requestBytes, OneShotResponseConsumer&& responseConsumer
)
{
    return std::make_unique<SendRequestTask>(
        socketIdentifier, sequenceNumber, std::move(requestBytes), std::move(responseConsumer)
    );
}

}
#endif
