#include "dansandu/farseer/internal/windows/register_message_consumer_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::asynchronous_operation::initialCompletionKey;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::socket_service::SocketServiceContainer;

namespace dansandu::farseer::internal::windows::register_message_consumer_asynchronous_operation
{

class RegisterMessageConsumerAsynchronousOperation : public AsynchronousOperation
{
public:
    RegisterMessageConsumerAsynchronousOperation(const SocketServiceId serviceId,
                                                 const ProtocolIdentifier protocolIdentifier,
                                                 UniqueFunction<void(std::any&&)>&& messageConsumer)
        : AsynchronousOperation{serviceId},
          protocolIdentifier_{protocolIdentifier},
          messageConsumer_{std::move(messageConsumer)}
    {
    }

    void postToCompletionPort(SocketServiceContainer& services,
                              IAsynchronousOperationsScheduler& asynchronousOperationsScheduler,
                              const HANDLE completionPort) override
    {
        const auto numberOfBytesTransferred = 0;
        const auto postResult =
            ::PostQueuedCompletionStatus(completionPort, numberOfBytesTransferred, initialCompletionKey, &overlapped_);

        if (!postResult)
        {
            THROW(std::runtime_error, "Posting ", getName(), " failed with error ", getLastErrorMessage());
        }
    }

    bool finalize(Sequencer<SocketServiceId>& sequencer, SocketServiceContainer& socketServiceContainer,
                  IAsynchronousOperationsScheduler& asynchronousOperationsScheduler, const HANDLE completionPort,
                  const DWORD numberOfBytesTransferred) override
    {
        const auto servicePosition = getServiceOrThrow(socketServiceContainer, serviceId_);

        servicePosition->second.protocolReader.registerMessageConsumer(protocolIdentifier_,
                                                                       std::move(messageConsumer_));

        LOG_INFO("Registered message consumer with protocol ID ", protocolIdentifier_.getUnderlying(),
                 " and socket service ID ", serviceId_.getUnderlying());

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
createRegisterMessageConsumerAsynchronousOperation(const SocketServiceId serviceId,
                                                   const ProtocolIdentifier protocolIdentifier,
                                                   UniqueFunction<void(std::any&&)>&& messageConsumer)
{
    return std::make_unique<RegisterMessageConsumerAsynchronousOperation>(serviceId, protocolIdentifier,
                                                                          std::move(messageConsumer));
}

}
