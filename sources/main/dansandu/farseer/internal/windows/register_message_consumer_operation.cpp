#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/register_message_consumer_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation_scheduler::defaultCompletionKey;
using dansandu::farseer::internal::windows::operation_scheduler::IOperationScheduler;
using dansandu::farseer::internal::windows::operation_scheduler::Operation;

namespace dansandu::farseer::internal::windows::register_message_consumer_operation
{

class RegisterMessageConsumerOperation : public Operation
{
public:
    RegisterMessageConsumerOperation(const SocketIdentifier socketIdentifier,
                                     const ProtocolIdentifier protocolIdentifier,
                                     UniqueFunction<void(std::any&&)>&& messageConsumer)
        : Operation{socketIdentifier},
          protocolIdentifier_{protocolIdentifier},
          messageConsumer_{std::move(messageConsumer)}
    {
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

    bool finalize(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
    {
        auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

        socket.protocolReader.registerMessageConsumer(protocolIdentifier_, std::move(messageConsumer_));

        LOG_INFO("Registered message consumer with protocol ID ", protocolIdentifier_.getUnderlying(),
                 " and socket socket ID ", socketIdentifier_.getUnderlying());

        return true;
    }

    const char* getName() const override
    {
        return "RegisterMessageConsumerOperation";
    }

private:
    const ProtocolIdentifier protocolIdentifier_;
    UniqueFunction<void(std::any&&)> messageConsumer_;
};

std::unique_ptr<Operation> createRegisterMessageConsumerOperation(const SocketIdentifier socketIdentifier,
                                                                  const ProtocolIdentifier protocolIdentifier,
                                                                  UniqueFunction<void(std::any&&)>&& messageConsumer)
{
    return std::make_unique<RegisterMessageConsumerOperation>(socketIdentifier, protocolIdentifier,
                                                              std::move(messageConsumer));
}

}
#endif
