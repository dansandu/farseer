#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/receive_operation.hpp"
#include "dansandu/journey/common.hpp"

using dansandu::farseer::internal::windows::operation_scheduler::IOperationScheduler;
using dansandu::farseer::internal::windows::operation_scheduler::Operation;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::receive_operation
{

constexpr auto maximumReceiveBufferSize = 4096;

class ReceiveOperation : public Operation
{
public:
    explicit ReceiveOperation(const SocketIdentifier socketIdentifier) : Operation{socketIdentifier}
    {
    }

    void postToCompletionPort(IOperationScheduler& operationScheduler) override
    {
        auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

        socket.socket.postReceive(receiveBuffer_, std::size(receiveBuffer_), &overlapped_);
    }

    bool finalize(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
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

            return false;
        }
        else
        {
            operationScheduler.eraseSocket(socketIdentifier_);

            return true;
        }
    }

    const char* getName() const override
    {
        return "ReceiveOperation";
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

std::unique_ptr<Operation> createReceiveOperation(const SocketIdentifier socketIdentifier)
{
    return std::make_unique<ReceiveOperation>(socketIdentifier);
}

}
#endif
