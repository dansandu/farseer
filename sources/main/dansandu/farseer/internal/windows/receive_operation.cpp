#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/receive_operation.hpp"
#include "dansandu/journey/common.hpp"

using dansandu::farseer::internal::windows::operation::INonUserOperation;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::maximumReceiveBufferSize;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::receive_operation
{

class ReceiveOperation : public INonUserOperation
{
public:
    explicit ReceiveOperation(const SocketIdentifier socketIdentifier) : socketIdentifier_{socketIdentifier}
    {
    }

    const char* getName() const override
    {
        return "ReceiveOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    Level getLoggingLevelFromErrorCode(const DWORD errorCode) const override
    {
        if (errorCode == ERROR_NETNAME_DELETED || errorCode == ERROR_CONNECTION_ABORTED)
        {
            return Level::info;
        }
        return Level::error;
    }

    void schedule(IOperationScheduler& operationScheduler) override
    {
        auto& socket = operationScheduler.getSocketOrThrow(socketIdentifier_);

        socket.socket.postReceive(receiveBuffer_, std::size(receiveBuffer_), this);
    }

    bool execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
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

            eraseWsaOverlapped();

            socket.socket.postReceive(receiveBuffer_, std::size(receiveBuffer_), this);

            return false;
        }
        else
        {
            operationScheduler.eraseSocket(socketIdentifier_);

            return true;
        }
    }

private:
    const SocketIdentifier socketIdentifier_;
    char receiveBuffer_[maximumReceiveBufferSize];
};

std::unique_ptr<INonUserOperation> createReceiveOperation(const SocketIdentifier socketIdentifier)
{
    return std::make_unique<ReceiveOperation>(socketIdentifier);
}

}
#endif
