#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/register_message_consumer_user_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::IUserOperation;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::register_message_consumer_user_operation
{

class RegisterMessageConsumerUserOperation : public IUserOperation
{
public:
    RegisterMessageConsumerUserOperation(
        const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
        UniqueFunction<void(std::any&&)>&& messageConsumer
    )
        : socketIdentifier_{socketIdentifier},
          protocolIdentifier_{protocolIdentifier},
          messageConsumer_{std::move(messageConsumer)}
    {
    }

    const char* getName() const override
    {
        return "RegisterMessageConsumerUserOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    bool execute(
        IOperationScheduler& operationScheduler, SocketContainer& socketContainer, const DWORD numberOfBytesTransferred
    ) override
    {
        auto& socket = socketContainer.getSocketOrThrow(socketIdentifier_);

        socket.protocolReader.registerInboundMessageConsumer(protocolIdentifier_, std::move(messageConsumer_));

        LOG_INFO(
            "Registered message consumer with protocol ID ", protocolIdentifier_, " and socket ID ", socketIdentifier_
        );

        return true;
    }

private:
    const SocketIdentifier socketIdentifier_;
    const ProtocolIdentifier protocolIdentifier_;
    UniqueFunction<void(std::any&&)> messageConsumer_;
};

std::unique_ptr<IUserOperation> createRegisterMessageConsumerUserOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<void(std::any&&)>&& messageConsumer
)
{
    return std::make_unique<RegisterMessageConsumerUserOperation>(
        socketIdentifier, protocolIdentifier, std::move(messageConsumer)
    );
}

}
#endif
