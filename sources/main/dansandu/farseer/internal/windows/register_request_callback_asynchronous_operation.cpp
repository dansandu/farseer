#include "dansandu/farseer/internal/windows/register_request_callback_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;

namespace dansandu::farseer::internal::windows::register_request_callback_asynchronous_operation
{

class RegisterRequestCallbackAsynchronousOperation : public AsynchronousOperation
{
public:
    RegisterRequestCallbackAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                                 const ProtocolIdentifier protocolIdentifier,
                                                 UniqueFunction<std::any(std::any&&)>&& requestConsumer)
        : AsynchronousOperation{socketIdentifier},
          protocolIdentifier_{protocolIdentifier},
          requestConsumer_{std::move(requestConsumer)}
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

        socket.protocolReader.registerRequestConsumer(protocolIdentifier_, std::move(requestConsumer_));

        LOG_INFO("Registered request consumer with protocol ID ", protocolIdentifier_.getUnderlying(),
                 " and socket socket ID ", socketIdentifier_.getUnderlying());

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
createRegisterRequestCallbackAsynchronousOperation(const SocketIdentifier socketIdentifier,
                                                   const ProtocolIdentifier protocolIdentifier,
                                                   UniqueFunction<std::any(std::any&&)>&& requestConsumer)
{
    return std::make_unique<RegisterRequestCallbackAsynchronousOperation>(socketIdentifier, protocolIdentifier,
                                                                          std::move(requestConsumer));
}

}
