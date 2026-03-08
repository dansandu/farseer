#include "dansandu/farseer/internal/windows/receive_asynchronous_operation.hpp"
#include "dansandu/journey/common.hpp"

using dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation;
using dansandu::farseer::internal::windows::asynchronous_operation::IAsynchronousOperationsScheduler;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::receive_asynchronous_operation
{

constexpr auto maximumReceiveBufferSize = 4096;

class ReceiveAsynchronousOperation : public AsynchronousOperation
{
public:
    explicit ReceiveAsynchronousOperation(const SocketIdentifier socketIdentifier)
        : AsynchronousOperation{socketIdentifier}
    {
    }

    void postToCompletionPort(IAsynchronousOperationsScheduler& asynchronousOperationsScheduler) override
    {
        auto& socket = asynchronousOperationsScheduler.getSocketOrThrow(socketIdentifier_);

        socket.socket.postReceive(receiveBuffer_, std::size(receiveBuffer_), &overlapped_);
    }

    bool finalize(IAsynchronousOperationsScheduler& asynchronousOperationsScheduler,
                  const DWORD numberOfBytesTransferred) override
    {
        if (numberOfBytesTransferred > 0)
        {
            auto& socket = asynchronousOperationsScheduler.getSocketOrThrow(socketIdentifier_);

            const auto listeningSocketIdentifier = socket.listeningSocketIdentifier;

            const auto bytes = std::span<uint8_t>(reinterpret_cast<uint8_t*>(receiveBuffer_), numberOfBytesTransferred);

            LOG_INFO("Socket with ID ", socketIdentifier_.getUnderlying(), " and address ",
                     socket.socket.getIpAddress(), ':', socket.socket.getPort(), " received ", bytes.size(), " bytes");

            if (listeningSocketIdentifier != invalidSocketIdentifier)
            {
                auto& listeningSocket = asynchronousOperationsScheduler.getSocketOrThrow(listeningSocketIdentifier);

                listeningSocket.protocolReader.read(socketIdentifier_, bytes);
            }
            else
            {
                socket.protocolReader.read(socketIdentifier_, bytes);
            }

            SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));

            socket.socket.postReceive(receiveBuffer_, std::size(receiveBuffer_), &overlapped_);

            return false;
        }
        else
        {
            asynchronousOperationsScheduler.eraseSocket(socketIdentifier_);

            return true;
        }
    }

    const char* getName() const override
    {
        return "ReceiveAsynchronousOperation";
    }

    Level getSystemErrorCodeLevel(const DWORD errorCode) const override
    {
        if (errorCode == ERROR_NETNAME_DELETED || errorCode == ERROR_CONNECTION_ABORTED)
        {
            return Level::info;
        }
        return Level::error;
    }

private:
    char receiveBuffer_[maximumReceiveBufferSize];
};

std::unique_ptr<AsynchronousOperation> createReceiveAsynchronousOperation(const SocketIdentifier socketIdentifier)
{
    return std::make_unique<ReceiveAsynchronousOperation>(socketIdentifier);
}

}
