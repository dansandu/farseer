#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/operation.hpp"
#include "dansandu/ballotin/string.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/journey/logging.hpp"

using dansandu::ballotin::string::toWideString;
using dansandu::farseer::internal::windows::error::getErrorMessageFromCode;
using dansandu::farseer::internal::windows::error::isClientError;
using dansandu::farseer::internal::windows::socket_container::SocketContainer;
using dansandu::journey::Level;

namespace dansandu::farseer::internal::windows::operation
{

namespace
{

bool isExpectedDisconnectError(const DWORD errorCode)
{
    return errorCode == ERROR_OPERATION_ABORTED || isClientError(errorCode);
}

}

void IOperation::onFailure(
    IOperationScheduler& operationScheduler, SocketContainer& socketContainer, const std::optional<DWORD> errorCode,
    const std::optional<std::wstring_view> errorMessage
)
{
    if (errorCode.has_value() || errorMessage.has_value())
    {
        // Clients disconnecting abruptly or cancelled operations can trigger these errors. Log these errors at the INFO
        // level. Tests will fail if any errors are logged during their execution.
        const auto isInfo = (errorCode.has_value() && isExpectedDisconnectError(*errorCode));

        const auto level = isInfo ? Level::info : Level::error;

        // When logging at the INFO level, prefer the succinct message from errorCode.
        // When logging at the ERROR level, prefer errorMessage because it contains more context, and, in case of
        // exceptions, it also contains the call stack.
        const auto message = isInfo || !errorMessage.has_value() ? toWideString(getErrorMessageFromCode(*errorCode))
                                                                 : std::wstring{*errorMessage};

        LOG(level, getName(), " with socket ID ", getSocketIdentifier(), " failed: ", message);
    }

    socketContainer.eraseSocket(getSocketIdentifier());
}

void IOperation::eraseWsaOverlapped() noexcept
{
    SecureZeroMemory(static_cast<WSAOVERLAPPED*>(this), sizeof(WSAOVERLAPPED));
}

}
#endif
