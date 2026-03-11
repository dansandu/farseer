#include "dansandu/farseer/internal/windows/register_message_consumer_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;

namespace dansandu::farseer::internal::windows::register_message_consumer_asynchronous_operation
{

class RegisterMessageConsumerAsynchronousOperation : public AsynchronousOperation
{
public:
    RegisterMessageConsumerAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                                 const ProtocolIdentifier protocolIdentifier,
                                                 UniqueFunction<void(std::any&&)>&& messageConsumer)
        : AsynchronousOperation{socketIdentifier},
          protocolIdentifier_{protocolIdentifier},
          messageConsumer_{std::move(messageConsumer)}
    {
    }

    void postToCompletionPort(IAsynchronousOperationsScheduler& asynchronousOperationsScheduler) override
    {
        const auto completionPort = asynchronousOperationsScheduler.getCompletionPort();

        const auto numberOfBytesTransferred = 0;
        const auto postResult =
            ::PostQueuedCompletionStatus(completionPort, numberOfBytesTransferred, defaultCompletionKey, &overlapped_);

        if (!postResult)
        {
            THROW(std::runtime_error, "Posting ", getName(), " failed with error ", getLastErrorMessage());
        }
    }

    bool finalize(IAsynchronousOperationsScheduler& asynchronousOperationsScheduler,
                  const DWORD numberOfBytesTransferred) override
    {
        auto& socket = asynchronousOperationsScheduler.getSocketOrThrow(socketIdentifier_);

        socket.protocolReader.registerMessageConsumer(protocolIdentifier_, std::move(messageConsumer_));

        LOG_INFO("Registered message consumer with protocol ID ", protocolIdentifier_.getUnderlying(),
                 " and socket socket ID ", socketIdentifier_.getUnderlying());

        return true;
    }

    const char* getName() const override
    {
        return "RegisterMessageConsumerAsynchronousOperation";
    }

private:
    const ProtocolIdentifier protocolIdentifier_;
    UniqueFunction<void(std::any&&)> messageConsumer_;
};

std::unique_ptr<AsynchronousOperation>
createRegisterMessageConsumerAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                                   const ProtocolIdentifier protocolIdentifier,
                                                   UniqueFunction<void(std::any&&)>&& messageConsumer)
{
    return std::make_unique<RegisterMessageConsumerAsynchronousOperation>(socketIdentifier, protocolIdentifier,
                                                                          std::move(messageConsumer));
}

}
