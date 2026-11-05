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
        const SocketIdentifier socketIdentifier, const ProtocolIdentifier responseIdentifier,
        const ProtocolSequenceNumber sequenceNumber,
        const std::optional<std::chrono::steady_clock::time_point> deadline, std::vector<uint8_t>&& bytes,
        UniqueFunction<void(std::any&&)>&& responseConsumer
    )
        : socketIdentifier_{socketIdentifier},
          responseIdentifier_{responseIdentifier},
          sequenceNumber_{sequenceNumber},
          deadline_{deadline},
          bytes_{std::move(bytes)},
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
            socketIdentifier_, responseIdentifier_, sequenceNumber_, deadline_, bytes_, std::move(responseConsumer_)
        );
    }

private:
    const SocketIdentifier socketIdentifier_;
    const ProtocolIdentifier responseIdentifier_;
    const ProtocolSequenceNumber sequenceNumber_;
    const std::optional<std::chrono::steady_clock::time_point> deadline_;
    const std::vector<uint8_t> bytes_;
    UniqueFunction<void(std::any&&)> responseConsumer_;
};

std::unique_ptr<ITask> createSendRequestTask(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier responseIdentifier,
    const ProtocolSequenceNumber sequenceNumber, const std::optional<std::chrono::steady_clock::time_point> deadline,
    std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer
)
{
    return std::make_unique<SendRequestTask>(
        socketIdentifier, responseIdentifier, sequenceNumber, deadline, std::move(bytes), std::move(responseConsumer)
    );
}

}
#endif
