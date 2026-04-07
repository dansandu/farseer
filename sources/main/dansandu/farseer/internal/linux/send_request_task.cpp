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
    SendRequestTask(const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber protocolSequenceNumber,
                    std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& responseConsumer)
        : socketIdentifier_{socketIdentifier},
          protocolSequenceNumber_{protocolSequenceNumber},
          bytes_{std::move(bytes)},
          responseConsumer_{std::move(responseConsumer)}
    {
    }

    const char* getName() const override
    {
        return "SendRequestTask";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    void execute(SocketContainer& socketContainer) override
    {
        socketContainer.sendRequest(socketIdentifier_, protocolSequenceNumber_, bytes_, std::move(responseConsumer_));
    }

private:
    const SocketIdentifier socketIdentifier_;
    const ProtocolSequenceNumber protocolSequenceNumber_;
    const std::vector<uint8_t> bytes_;
    UniqueFunction<void(std::any&&)> responseConsumer_;
};

std::unique_ptr<ITask> createSendRequestTask(const SocketIdentifier socketIdentifier,
                                             const ProtocolSequenceNumber protocolSequenceNumber,
                                             std::vector<uint8_t>&& bytes,
                                             UniqueFunction<void(std::any&&)>&& responseConsumer)
{
    return std::make_unique<SendRequestTask>(socketIdentifier, protocolSequenceNumber, std::move(bytes),
                                             std::move(responseConsumer));
}

}
#endif
