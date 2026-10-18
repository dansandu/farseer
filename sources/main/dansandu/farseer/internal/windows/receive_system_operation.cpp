#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/receive_system_operation.hpp"
#include "dansandu/journey/common.hpp"

using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::ISystemOperation;
using dansandu::farseer::internal::windows::operation::maximumReceiveBufferSize;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::receive_system_operation
{

class ReceiveSystemOperation : public ISystemOperation
{
public:
    explicit ReceiveSystemOperation(const SocketIdentifier socketIdentifier) : socketIdentifier_{socketIdentifier}
    {
    }

    const char* getName() const noexcept override
    {
        return "ReceiveSystemOperation";
    }

    SocketIdentifier getSocketIdentifier() const noexcept override
    {
        return socketIdentifier_;
    }

    void schedule(IOperationScheduler& operationScheduler, SocketContainer& socketContainer) override
    {
        auto& socket = socketContainer.getSocketOrThrow(socketIdentifier_);

        socket.socket.postReceive(receiveBuffer_, std::size(receiveBuffer_), this);
    }

    bool execute(
        IOperationScheduler& operationScheduler, SocketContainer& socketContainer, const DWORD numberOfBytesTransferred
    ) override
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

            auto& socket = socketContainer.getSocketOrThrow(socketIdentifier_);

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
                auto& listeningSocket = socketContainer.getSocketOrThrow(listeningSocketIdentifier);

                listeningSocket.protocolReader.readInboundBytes(socket.inboundBytes, socket.outboundBytes);
            }
            else
            {
                socket.protocolReader.readInboundBytes(socket.inboundBytes, socket.outboundBytes);
            }

            if (!socket.outboundBytes.empty())
            {
                operationScheduler.scheduleSendBytesSystemOperation(socketIdentifier_, std::move(socket.outboundBytes));

                socket.outboundBytes.clear();
            }

            eraseWsaOverlapped();

            socket.socket.postReceive(receiveBuffer_, std::size(receiveBuffer_), this);

            return false;
        }
        else
        {
            socketContainer.eraseSocket(socketIdentifier_);

            return true;
        }
    }

private:
    const SocketIdentifier socketIdentifier_;
    char receiveBuffer_[maximumReceiveBufferSize];
};

std::unique_ptr<ISystemOperation> createReceiveSystemOperation(const SocketIdentifier socketIdentifier)
{
    return std::make_unique<ReceiveSystemOperation>(socketIdentifier);
}

}
#endif
