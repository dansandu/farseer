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
    RegisterMessageConsumerOperation(const SocketIdentifier socketIdentifier,
                                     const ProtocolIdentifier protocolIdentifier,
                                     UniqueFunction<void(std::any&&)>&& messageConsumer)
        : socketIdentifier_{socketIdentifier},
          protocolIdentifier_{protocolIdentifier},
          messageConsumer_{std::move(messageConsumer)}
    {
        SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));
    }

    const char* getName() const override
    {
        return "RegisterMessageConsumerOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    Level getSystemErrorCodeLevel(const DWORD errorCode) const override
    {
        return Level::error;
    }

    bool discard(const DWORD numberOfBytesTransferred) const override
    {
        return true;
    }

    LPWSAOVERLAPPED getOverlapped() override
    {
        return &overlapped_;
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

        socket.protocolReader.registerMessageConsumer(protocolIdentifier_, std::move(messageConsumer_));

        LOG_INFO("Registered message consumer with protocol ID ", protocolIdentifier_.getUnderlying(),
                 " and socket socket ID ", socketIdentifier_.getUnderlying());
    }

private:
    const SocketIdentifier socketIdentifier_;
    const ProtocolIdentifier protocolIdentifier_;
    UniqueFunction<void(std::any&&)> messageConsumer_;
    WSAOVERLAPPED overlapped_;
};

std::unique_ptr<IOperation> createRegisterMessageConsumerOperation(const SocketIdentifier socketIdentifier,
                                                                   const ProtocolIdentifier protocolIdentifier,
                                                                   UniqueFunction<void(std::any&&)>&& messageConsumer)
{
    return std::make_unique<RegisterMessageConsumerOperation>(socketIdentifier, protocolIdentifier,
                                                              std::move(messageConsumer));
}

}
#endif
