#include "dansandu/farseer/internal/windows/register_message_consumer_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsFactory;
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
                                                 std::function<void(std::any)> messageConsumer)
        : AsynchronousOperation{serviceId},
          protocolIdentifier_{protocolIdentifier},
          messageConsumer_{std::move(messageConsumer)}
    {
    }

    void postToCompletionPort(SocketServiceContainer& services, const HANDLE completionPort) override
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
                  IAsynchronousOperationsFactory& asynchronousOperationsFactory, const HANDLE completionPort,
                  const DWORD numberOfBytesTransferred) override
    {
        // register to listening socket or to accepted socket
        const auto servicePosition = getServiceOrThrow(socketServiceContainer, serviceId_);

        servicePosition->second.protocolReader.registerProtocolConsumer(protocolIdentifier_,
                                                                        std::move(messageConsumer_));

        LOG_INFO("Registered message consumer with protocol ID ", protocolIdentifier_.getInteger(),
                 " and socket service ID ", serviceId_.getInteger());

        return true;
    }

    const char* getName() const override
    {
        return "RegisterMessageConsumerAsynchronousOperation";
    }

private:
    const ProtocolIdentifier protocolIdentifier_;
    std::function<void(std::any)> messageConsumer_;
};

std::unique_ptr<AsynchronousOperation>
createRegisterMessageConsumerAsynchronousOperation(const SocketServiceId serviceId,
                                                   const ProtocolIdentifier protocolIdentifier,
                                                   std::function<void(std::any)> messageConsumer)
{
    return std::make_unique<RegisterMessageConsumerAsynchronousOperation>(serviceId, protocolIdentifier,
                                                                          std::move(messageConsumer));
}

}
