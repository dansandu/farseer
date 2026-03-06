#include "dansandu/farseer/internal/windows/register_request_callback_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::asynchronous_operation::initialCompletionKey;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::socket_service::SocketServiceContainer;

namespace dansandu::farseer::internal::windows::register_request_callback_asynchronous_operation
{

class RegisterRequestCallbackAsynchronousOperation : public AsynchronousOperation
{
public:
    RegisterRequestCallbackAsynchronousOperation(const SocketServiceId serviceId,
                                                 const ProtocolIdentifier protocolIdentifier,
                                                 UniqueFunction<std::any(std::any&&)>&& requestConsumer)
        : AsynchronousOperation{serviceId},
          protocolIdentifier_{protocolIdentifier},
          requestConsumer_{std::move(requestConsumer)}
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

        servicePosition->second.protocolReader.registerRequestConsumer(protocolIdentifier_,
                                                                       std::move(requestConsumer_));

        LOG_INFO("Registered request consumer with protocol ID ", protocolIdentifier_.getUnderlying(),
                 " and socket service ID ", serviceId_.getUnderlying());

        return true;
    }

    const char* getName() const override
    {
        return "RegisterRequestCallbackAsynchronousOperation";
    }

private:
    const ProtocolIdentifier protocolIdentifier_;
    UniqueFunction<std::any(std::any&&)> requestConsumer_;
};

std::unique_ptr<AsynchronousOperation>
createRegisterRequestCallbackAsynchronousOperation(const SocketServiceId serviceId,
                                                   const ProtocolIdentifier protocolIdentifier,
                                                   UniqueFunction<std::any(std::any&&)>&& requestConsumer)
{
    return std::make_unique<RegisterRequestCallbackAsynchronousOperation>(serviceId, protocolIdentifier,
                                                                          std::move(requestConsumer));
}

}
