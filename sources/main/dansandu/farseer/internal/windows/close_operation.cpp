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
    }

    const char* getName() const override
    {
        return "CloseOperation";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    Level getLoggingLevelFromErrorCode(const DWORD errorCode) const override
    {
        return Level::error;
    }

    bool execute(IOperationScheduler& operationScheduler, const DWORD numberOfBytesTransferred) override
    {
        operationScheduler.eraseSocket(socketIdentifier_);

        return true;
    }

private:
    SocketIdentifier socketIdentifier_;
};

std::unique_ptr<IOperation> createCloseOperation(const SocketIdentifier socketIdentifier)
{
    return std::make_unique<CloseOperation>(socketIdentifier);
}

}
#endif
