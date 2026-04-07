#if defined(__linux__)
#include "dansandu/farseer/internal/linux/register_message_consumer_task.hpp"
#include "dansandu/farseer/common.hpp"

using dansandu::farseer::internal::linux::socket_container::SocketContainer;
using dansandu::farseer::internal::linux::task::ITask;

namespace dansandu::farseer::internal::linux::register_message_consumer_task
{

class RegisterMessageConsumerTask : public ITask
{
public:
    RegisterMessageConsumerTask(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                UniqueFunction<void(std::any&&)>&& messageConsumer)
        : socketIdentifier_{socketIdentifier},
          protocolIdentifier_{protocolIdentifier},
          messageConsumer_{std::move(messageConsumer)}
    {
    }

    const char* getName() const override
    {
        return "RegisterMessageConsumerTask";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    void execute(SocketContainer& socketContainer) override
    {
        socketContainer.registerMessageConsumer(socketIdentifier_, protocolIdentifier_, std::move(messageConsumer_));
    }

private:
    const SocketIdentifier socketIdentifier_;
    const ProtocolIdentifier protocolIdentifier_;
    UniqueFunction<void(std::any&&)> messageConsumer_;
};

std::unique_ptr<ITask> createRegisterMessageConsumerTask(const SocketIdentifier socketIdentifier,
                                                         const ProtocolIdentifier protocolIdentifier,
                                                         UniqueFunction<void(std::any&&)>&& messageConsumer)
{
    return std::make_unique<RegisterMessageConsumerTask>(socketIdentifier, protocolIdentifier,
                                                         std::move(messageConsumer));
}

}
#endif
