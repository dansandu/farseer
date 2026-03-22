#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/register_request_callback_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::i_operation_scheduler::defaultCompletionKey;
using dansandu::farseer::internal::windows::i_operation_scheduler::IOperationScheduler;
using dansandu::farseer::internal::windows::i_operation_scheduler::Operation;

namespace dansandu::farseer::internal::windows::register_request_callback_operation
{

class RegisterRequestCallbackOperation : public Operation
{
public:
    RegisterRequestCallbackOperation(const SocketIdentifier socketIdentifier,
                                     const ProtocolIdentifier protocolIdentifier,
                                     UniqueFunction<std::any(std::any&&)>&& requestConsumer)
        : Operation{socketIdentifier},
          protocolIdentifier_{protocolIdentifier},
          requestConsumer_{std::move(requestConsumer)}
    {
    }

    const char* getName() const override
    {
        return "RegisterRequestCallbackOperation";
    }

    bool discard(const DWORD numberOfBytesTransferred) const
    {
        return true;
    }

    void postToCompletionPort(IOperationScheduler& operationScheduler) override
    {
        const auto completionPort = operationScheduler.getCompletionPort();

        const auto numberOfBytesTransferred = 0;
        const auto postResult =
            ::PostQueuedCompletionStatus(completionPort, numberOfBytesTransferred, defaultCompletionKey, &overlapped_);

        if (!postResult)
        {
            THROW(std::runtime_error, "Posting ", getName(), " failed with error ", getLastErrorMessage());
        }
    }

    void execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
    {
        auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

        socket.protocolReader.registerRequestConsumer(protocolIdentifier_, std::move(requestConsumer_));

        LOG_INFO("Registered request consumer with protocol ID ", protocolIdentifier_.getUnderlying(),
                 " and socket socket ID ", socketIdentifier_.getUnderlying());
    }

private:
    const ProtocolIdentifier protocolIdentifier_;
    UniqueFunction<std::any(std::any&&)> requestConsumer_;
};

std::unique_ptr<Operation>
createRegisterRequestCallbackOperation(const SocketIdentifier socketIdentifier,
                                       const ProtocolIdentifier protocolIdentifier,
                                       UniqueFunction<std::any(std::any&&)>&& requestConsumer)
{
    return std::make_unique<RegisterRequestCallbackOperation>(socketIdentifier, protocolIdentifier,
                                                              std::move(requestConsumer));
}

}
#endif
