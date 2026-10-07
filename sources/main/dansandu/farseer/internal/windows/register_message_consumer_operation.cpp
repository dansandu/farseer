#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/register_message_consumer_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::operation::IOperation;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::register_message_consumer_operation
{

class RegisterMessageConsumerOperation : public IOperation
{
public:
    RegisterMessageConsumerOperation(
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
        return "RegisterMessageConsumerOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    Level getLoggingLevelFromErrorCode(const DWORD errorCode) const override
    {
        return Level::error;
    }

    bool execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
    {
        auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

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

std::unique_ptr<IOperation> createRegisterMessageConsumerOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<void(std::any&&)>&& messageConsumer
)
{
    return std::make_unique<RegisterMessageConsumerOperation>(
        socketIdentifier, protocolIdentifier, std::move(messageConsumer)
    );
}

}
#endif
