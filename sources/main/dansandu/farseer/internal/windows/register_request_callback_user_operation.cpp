#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/register_request_callback_user_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::IUserOperation;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::register_request_callback_user_operation
{

class RegisterRequestCallbackUserOperation : public IUserOperation
{
public:
    RegisterRequestCallbackUserOperation(
        const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
        UniqueFunction<std::any(std::any&&)>&& requestConsumer
    )
        : socketIdentifier_{socketIdentifier},
          protocolIdentifier_{protocolIdentifier},
          requestConsumer_{std::move(requestConsumer)}
    {
    }

    const char* getName() const override
    {
        return "RegisterRequestCallbackUserOperation";
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

        socket.protocolReader.registerInboundRequestConsumer(protocolIdentifier_, std::move(requestConsumer_));

        LOG_INFO(
            "Registered request consumer with protocol ID ", protocolIdentifier_, " and socket ID ", socketIdentifier_
        );

        return true;
    }

private:
    const SocketIdentifier socketIdentifier_;
    const ProtocolIdentifier protocolIdentifier_;
    UniqueFunction<std::any(std::any&&)> requestConsumer_;
};

std::unique_ptr<IUserOperation> createRegisterRequestCallbackUserOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<std::any(std::any&&)>&& requestConsumer
)
{
    return std::make_unique<RegisterRequestCallbackUserOperation>(
        socketIdentifier, protocolIdentifier, std::move(requestConsumer)
    );
}

}
#endif
