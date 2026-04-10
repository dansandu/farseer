#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/close_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::defaultCompletionKey;
using dansandu::farseer::internal::windows::operation::IOperation;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::close_operation
{

class CloseOperation : public IOperation
{
public:
    explicit CloseOperation(const SocketIdentifier socketIdentifier) : socketIdentifier_{socketIdentifier}
    {
        SecureZeroMemory(&overlapped_, sizeof(WSAOVERLAPPED));
    }

    const char* getName() const override
    {
        return "CloseOperation";
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
        operationScheduler.eraseSocket(socketIdentifier_);
    }

private:
    SocketIdentifier socketIdentifier_;
    WSAOVERLAPPED overlapped_;
};

std::unique_ptr<IOperation> createCloseOperation(const SocketIdentifier socketIdentifier)
{
    return std::make_unique<CloseOperation>(socketIdentifier);
}

}
#endif
