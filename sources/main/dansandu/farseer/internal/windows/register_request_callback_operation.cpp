#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/register_request_callback_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::operation::IOperation;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::register_request_callback_operation
{

class RegisterRequestCallbackOperation : public IOperation
{
public:
    RegisterRequestCallbackOperation(
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
        return "RegisterRequestCallbackOperation";
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

std::unique_ptr<IOperation> createRegisterRequestCallbackOperation(
    const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<std::any(std::any&&)>&& requestConsumer
)
{
    return std::make_unique<RegisterRequestCallbackOperation>(
        socketIdentifier, protocolIdentifier, std::move(requestConsumer)
    );
}

}
#endif
