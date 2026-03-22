#pragma once

#include "dansandu/farseer/internal/windows/i_operation_scheduler.hpp"

#include <map>
#include <memory>
#include <mutex>

namespace dansandu::farseer::internal::windows::operation_container
{

class OperationContainer
{
public:
    void insert(std::unique_ptr<dansandu::farseer::internal::windows::i_operation_scheduler::Operation>&& operation,
                dansandu::farseer::internal::windows::i_operation_scheduler::IOperationScheduler& operationScheduler);

    void handleSuccessfulOperation(
        const LPWSAOVERLAPPED overlapped, const DWORD numberOfBytesTransferred,
        dansandu::farseer::internal::windows::i_operation_scheduler::IOperationScheduler& operationScheduler);

    void handleFailedOperation(const LPWSAOVERLAPPED overlapped, const DWORD errorCode);

private:
    void
    handleOperationExecutionFailure(dansandu::farseer::internal::windows::i_operation_scheduler::Operation& operation,
                                    const bool discarded, const std::wstring_view message);

    std::map<LPWSAOVERLAPPED, std::unique_ptr<dansandu::farseer::internal::windows::i_operation_scheduler::Operation>>
        operations_;
    std::mutex mutex_;
};

}
