#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/operation_container.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/journey/exception.hpp"

using dansandu::ballotin::string::toWideString;
using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::windows::error::getErrorMessageFromCode;
using dansandu::farseer::internal::windows::operation::IOperation;
using dansandu::farseer::internal::windows::operation::IOperationScheduler;
using dansandu::journey::exception::WideException;

namespace dansandu::farseer::internal::windows::operation_container
{

OperationContainer::OperationContainer(IOperationScheduler& operationScheduler)
    : operationScheduler_{operationScheduler}
{
}

void OperationContainer::insert(std::unique_ptr<IOperation>&& operation)
{
    const auto overlapped = operation->getOverlapped();
    const auto name = operation->getName();
    const auto socketIdentifier = operation->getSocketIdentifier();

    const auto lock = std::lock_guard<std::mutex>{mutex_};
    const auto [position, inserted] = operations_.insert({overlapped, std::move(operation)});

    if (!inserted)
    {
        THROW(std::logic_error, "Couldn't insert ", name, " with socket ID ", socketIdentifier,
              " because operation already exists");
    }

    SCOPE_FAILURE([&]() { operations_.erase(position); });

    position->second->postToCompletionPort(operationScheduler_);

    LOG_DEBUG("Inserted ", name, " with socket ID ", socketIdentifier);
}

void OperationContainer::handleSuccessfulOperation(const LPWSAOVERLAPPED overlapped,
                                                   const DWORD numberOfBytesTransferred)
{
    auto operationGuard = std::unique_ptr<IOperation>{};
    auto discard = false;

    IOperation* operation = nullptr;

    {
        const auto lock = std::lock_guard<std::mutex>{mutex_};

        const auto position = operations_.find(overlapped);

        if (position == operations_.cend())
        {
            LOG_ERROR("Couldn't find operation");
            return;
        }

        operation = position->second.get();
        discard = operation->discard(numberOfBytesTransferred);

        if (discard)
        {
            operationGuard = std::move(position->second);

            operations_.erase(position);

            LOG_DEBUG("Erased ", operation->getName(), " with socket ID ", operation->getSocketIdentifier());
        }
    }

    LOG_DEBUG("Executing ", operation->getName(), " with socket ID ", operation->getSocketIdentifier());

    try
    {
        operation->execute(operationScheduler_, numberOfBytesTransferred);
    }
    catch (const WideException& exception)
    {
        handleOperationExecutionFailure(*operation, discard, exception.getMessage());
    }
    catch (const std::exception& exception)
    {
        handleOperationExecutionFailure(*operation, discard, toWideString(exception.what()));
    }
}

void OperationContainer::handleFailedOperation(const LPWSAOVERLAPPED overlapped, const DWORD errorCode)
{
    const auto lock = std::lock_guard<std::mutex>{mutex_};
    const auto position = operations_.find(overlapped);

    if (position != operations_.cend())
    {
        const auto socketIdentifier = position->second->getSocketIdentifier();

        SCOPE_EXIT(
            [&]()
            {
                operations_.erase(position);
                operationScheduler_.eraseSocket(socketIdentifier);
            });

        const auto name = position->second->getName();
        const auto level = position->second->getSystemErrorCodeLevel(errorCode);
        const auto message = getErrorMessageFromCode(errorCode);

        LOG(level, name, " with socket ID ", socketIdentifier, " failed: ", message);
    }
    else
    {
        const auto errorMessage = getErrorMessageFromCode(errorCode);

        LOG_ERROR("Unknown operation failed: ", errorMessage);
    }
}

void OperationContainer::handleOperationExecutionFailure(IOperation& operation, const bool discarded,
                                                         const std::wstring_view message)
{
    const auto socketIdentifier = operation.getSocketIdentifier();

    SCOPE_EXIT(
        [&]()
        {
            if (!discarded)
            {
                const auto lock = std::lock_guard<std::mutex>{mutex_};
                const auto position = operations_.find(operation.getOverlapped());
                operations_.erase(position);
            }

            operationScheduler_.eraseSocket(socketIdentifier);
        });

    LOG_ERROR(operation.getName(), " with socket ID ", socketIdentifier, " failed: ", message);
}

}
#endif
