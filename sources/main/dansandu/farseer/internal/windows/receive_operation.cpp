#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/receive_operation.hpp"
#include "dansandu/journey/common.hpp"

using dansandu::farseer::internal::windows::operation::IOperation;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::receive_operation
{

class ReceiveOperation : public IOperation
{
public:
    explicit ReceiveOperation(const SocketIdentifier socketIdentifier) : socketIdentifier_{socketIdentifier}
    {
        SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));
    }

    const char* getName() const override
    {
        return "ReceiveOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    Level getSystemErrorCodeLevel(const DWORD errorCode) const override
    {
        if (errorCode == ERROR_NETNAME_DELETED || errorCode == ERROR_CONNECTION_ABORTED)
        {
            return Level::info;
        }
        return Level::error;
    }

    bool discard(const DWORD numberOfBytesTransferred) const override
    {
        return numberOfBytesTransferred <= 0;
    }

    LPWSAOVERLAPPED getOverlapped() override
    {
        return &overlapped_;
    }

    void postToCompletionPort(IOperationScheduler& operationScheduler) override
    {
        auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

        socket.socket.postReceive(receiveBuffer_, std::size(receiveBuffer_), &overlapped_);
    }

    void execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
    {
        if (numberOfBytesTransferred > 0)
        {
            auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

            const auto listeningSocketIdentifier = socket.listeningSocketIdentifier;

            const auto bytes = std::span<uint8_t>(reinterpret_cast<uint8_t*>(receiveBuffer_), numberOfBytesTransferred);

            LOG_INFO("Socket with ID ", socketIdentifier_.getUnderlying(), " and address ",
                     socket.socket.getIpAddress(), ':', socket.socket.getPort(), " received ", bytes.size(), " bytes");

            if (listeningSocketIdentifier != invalidSocketIdentifier)
            {
                auto& listeningSocket = operationScheduler.getSocketOrThrow(listeningSocketIdentifier);

                listeningSocket.protocolReader.read(socketIdentifier_, bytes);
            }
            else
            {
                socket.protocolReader.read(socketIdentifier_, bytes);
            }

            SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));

            socket.socket.postReceive(receiveBuffer_, std::size(receiveBuffer_), &overlapped_);
        }
        else
        {
            operationScheduler.eraseSocket(socketIdentifier_);
        }
    }

private:
    static constexpr auto maximumReceiveBufferSize = 4096;

    const SocketIdentifier socketIdentifier_;
    char receiveBuffer_[maximumReceiveBufferSize];
    WSAOVERLAPPED overlapped_;
};

std::unique_ptr<IOperation> createReceiveOperation(const SocketIdentifier socketIdentifier)
{
    return std::make_unique<ReceiveOperation>(socketIdentifier);
}

}
#endif
