#pragma once

#include "dansandu/farseer/internal/windows/operation.hpp"

#include <map>
#include <memory>
#include <mutex>

namespace dansandu::farseer::internal::windows::operation_container
{

class OperationContainer
{
public:
    OperationContainer(const OperationContainer& other) = delete;
    OperationContainer(OperationContainer&& other) noexcept = delete;
    OperationContainer& operator=(const OperationContainer& other) = delete;
    OperationContainer& operator=(OperationContainer&& other) noexcept = delete;

    explicit OperationContainer(
        dansandu::farseer::internal::windows::operation::IOperationScheduler& operationScheduler);

    void insert(std::unique_ptr<dansandu::farseer::internal::windows::operation::IOperation>&& operation);

    void handleSuccessfulOperation(const LPWSAOVERLAPPED overlapped, const DWORD numberOfBytesTransferred);

    void handleFailedOperation(const LPWSAOVERLAPPED overlapped, const DWORD errorCode);

private:
    void handleOperationExecutionFailure(dansandu::farseer::internal::windows::operation::IOperation& operation,
                                         const bool discarded, const std::wstring_view message);

    dansandu::farseer::internal::windows::operation::IOperationScheduler& operationScheduler_;
    std::map<LPWSAOVERLAPPED, std::unique_ptr<dansandu::farseer::internal::windows::operation::IOperation>> operations_;
    std::mutex mutex_;
};

}
