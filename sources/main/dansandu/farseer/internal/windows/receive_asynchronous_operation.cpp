#include "dansandu/farseer/internal/windows/receive_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"

using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsRegistry;
using dansandu::farseer::internal::windows::socket_service::SocketServiceContainer;

namespace dansandu::farseer::internal::windows::receive_asynchronous_operation
{

constexpr auto maximumReceiveBufferSize = 4096;

class ReceiveAsynchronousOperation : public AsynchronousOperation
{
public:
    explicit ReceiveAsynchronousOperation(const SocketServiceId serviceId) : AsynchronousOperation{serviceId}
    {
    }

    void postToCompletionPort(SocketServiceContainer& services, const HANDLE completionPort) override
    {
        const auto position = getServiceOrThrow(services, serviceId_);

        position->second.socket.postReceive(receiveBuffer_, std::size(receiveBuffer_), &overlapped_);
    }

    bool finalize(Sequencer<SocketServiceId>& sequencer, SocketServiceContainer& socketServiceContainer,
                  IAsynchronousOperationsRegistry& asynchronousOperationsRegistry, const HANDLE completionPort,
                  const DWORD numberOfBytesTransferred) override
    {
        if (numberOfBytesTransferred > 0)
        {
            const auto position = getServiceOrThrow(socketServiceContainer, serviceId_);

            const auto listeningServiceId = position->second.listeningServiceId;

            const auto& socket = position->second.socket;

            const auto bytes = std::span<uint8_t>(reinterpret_cast<uint8_t*>(receiveBuffer_), numberOfBytesTransferred);

            LOG_INFO("Socket with ID ", serviceId_.getInteger(), " and address ", socket.getIpAddress(), ':',
                     socket.getPort(), " received ", bytes.size(), " bytes");

            if (listeningServiceId != InvalidServiceId)
            {
                const auto listeningServicePosition = getServiceOrThrow(socketServiceContainer, listeningServiceId);

                listeningServicePosition->second.protocolReader.read(bytes);
            }
            else
            {
                position->second.protocolReader.read(bytes);
            }

            SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));

            position->second.socket.postReceive(receiveBuffer_, std::size(receiveBuffer_), &overlapped_);

            return false;
        }
        else
        {
            closeSocketService(socketServiceContainer, serviceId_);

            return true;
        }
    }

    const char* getName() const override
    {
        return "ReceiveAsynchronousOperation";
    }

private:
    char receiveBuffer_[maximumReceiveBufferSize];
};

std::unique_ptr<AsynchronousOperation> createReceiveAsynchronousOperation(const SocketServiceId serviceId)
{
    return std::make_unique<ReceiveAsynchronousOperation>(serviceId);
}

}
