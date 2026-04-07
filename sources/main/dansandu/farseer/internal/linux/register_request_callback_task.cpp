#if defined(__linux__)
#include "dansandu/farseer/internal/linux/register_request_callback_task.hpp"
#include "dansandu/farseer/common.hpp"

using dansandu::farseer::internal::linux::socket_container::SocketContainer;
using dansandu::farseer::internal::linux::task::ITask;

namespace dansandu::farseer::internal::linux::register_request_callback_task
{

class RegisterRequestCallbackTask : public ITask
{
public:
    RegisterRequestCallbackTask(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                UniqueFunction<std::any(std::any&&)>&& requestConsumer)
        : socketIdentifier_{socketIdentifier},
          protocolIdentifier_{protocolIdentifier},
          requestConsumer_{std::move(requestConsumer)}
    {
    }

    const char* getName() const override
    {
        return "RegisterRequestCallbackTask";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    void execute(SocketContainer& socketContainer) override
    {
        socketContainer.registerRequestCallback(socketIdentifier_, protocolIdentifier_, std::move(requestConsumer_));
    }

private:
    const SocketIdentifier socketIdentifier_;
    const ProtocolIdentifier protocolIdentifier_;
    UniqueFunction<std::any(std::any&&)> requestConsumer_;
};

std::unique_ptr<ITask> createRegisterRequestCallbackTask(const SocketIdentifier socketIdentifier,
                                                         const ProtocolIdentifier protocolIdentifier,
                                                         UniqueFunction<std::any(std::any&&)>&& requestConsumer)
{
    return std::make_unique<RegisterRequestCallbackTask>(socketIdentifier, protocolIdentifier,
                                                         std::move(requestConsumer));
}

}
#endif
