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
            if (numberOfBytesTransferred > maximumReceiveBufferSize)
            {
                THROW(
                    std::logic_error, "The number of bytes received ", numberOfBytesTransferred,
                    " exceeds the maximum buffer size ", maximumReceiveBufferSize
                );
            }

            auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

            const auto listeningSocketIdentifier = socket.listeningSocketIdentifier;

            const auto bytesBegin = reinterpret_cast<uint8_t*>(receiveBuffer_);

            const auto bytesEnd = bytesBegin + numberOfBytesTransferred;

            socket.inboundBytes.insert(socket.inboundBytes.end(), bytesBegin, bytesEnd);

            LOG_INFO(
                "Socket with ID ", socketIdentifier_, " and address ", socket.socket.getIpAddress(), ':',
                socket.socket.getPort(), " received ", numberOfBytesTransferred, " bytes"
            );

            if (listeningSocketIdentifier != invalidSocketIdentifier)
            {
                auto& listeningSocket = operationScheduler.getSocketOrThrow(listeningSocketIdentifier);

                listeningSocket.protocolReader.readInboundBytes(socket.inboundBytes, socket.outboundBytes);
            }
            else
            {
                socket.protocolReader.readInboundBytes(socket.inboundBytes, socket.outboundBytes);
            }

            if (!socket.outboundBytes.empty())
            {
                operationScheduler.scheduleSendBytesOperation(socketIdentifier_, std::move(socket.outboundBytes));

                socket.outboundBytes.clear();
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
    static constexpr DWORD maximumReceiveBufferSize = 4096;

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
