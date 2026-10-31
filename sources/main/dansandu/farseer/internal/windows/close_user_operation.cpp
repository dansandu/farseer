#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/close_user_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"

using dansandu::farseer::internal::windows::error::getLastErrorMessage;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::farseer::internal::windows::operation::IUserOperation;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::close_user_operation
{

class CloseUserOperation : public IUserOperation
{
public:
    explicit CloseUserOperation(const SocketIdentifier socketIdentifier) : socketIdentifier_{socketIdentifier}
    {
    }

    const char* getName() const noexcept override
    {
        return "CloseUserOperation";
    }

    SocketIdentifier getSocketIdentifier() const noexcept override
    {
        return socketIdentifier_;
    }

    bool execute(
        IOperationScheduler& operationScheduler, SocketContainer& socketContainer, const DWORD numberOfBytesTransferred
    ) override
    {
        socketContainer.eraseSocket(socketIdentifier_);

        return false;
    }

private:
    SocketIdentifier socketIdentifier_;
};

std::unique_ptr<IUserOperation> createCloseUserOperation(const SocketIdentifier socketIdentifier)
{
    return std::make_unique<CloseUserOperation>(socketIdentifier);
}

}
#endif
