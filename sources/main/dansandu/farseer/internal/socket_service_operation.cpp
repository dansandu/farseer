#include "dansandu/farseer/internal/socket_service_operation.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/ballotin/logging.hpp"

#include <winsock2.h>

#include <stdexcept>

using dansandu::ballotin::logging::LogDebug;
using dansandu::ballotin::logging::LogError;

namespace dansandu::farseer::internal::socket_service_operation
{

const char* toString(const SocketServiceOperationType operationType)
{
    switch (operationType)
    {
    case SocketServiceOperationType::listen:
        return "listen";
    case SocketServiceOperationType::pendingAccept:
        return "pendingAccept";
    case SocketServiceOperationType::connect:
        return "connect";
    case SocketServiceOperationType::pendingConnect:
        return "pendingConnect";
    case SocketServiceOperationType::pendingReceiveMessage:
        return "pendingReceiveMessage";
    case SocketServiceOperationType::sendMessage:
        return "sendMessage";
    case SocketServiceOperationType::pendingSendMessage:
        return "pendingSendMessage";
    case SocketServiceOperationType::close:
        return "close";
    default:
        THROW(std::logic_error, "Unknown SocketServiceOperationType");
    }
}

SocketServiceOperation* SocketServiceOperationContainer::push(SocketServiceOperation operation)
{
    auto value = std::make_unique<SocketServiceOperation>(std::move(operation));

    const auto key = value.get();

    {
        const auto lock = std::lock_guard<std::mutex>{operationsLock_};
        operations_.insert({key, std::move(value)});
    }

    LogDebug("Pushed ", toString(key->operationType), " operation with memory address ", key, " and service ID ",
             key->serviceId.integer());

    return key;
}

std::unique_ptr<SocketServiceOperation> SocketServiceOperationContainer::pop(LPOVERLAPPED overlapped)
{
    auto operation = std::unique_ptr<SocketServiceOperation>{};

    {
        const auto lock = std::lock_guard<std::mutex>{operationsLock_};
        const auto operationPosition = operations_.find(overlapped);
        if (operationPosition != operations_.end())
        {
            operation = std::move(operationPosition->second);
            operations_.erase(operationPosition);
        }
    }

    if (operation)
    {
        LogDebug("Popped ", toString(operation->operationType), " operation with memory address ", operation.get(),
                 " and service ID ", operation->serviceId.integer());
    }
    else
    {
        LogError("Couldn't pop unknown operation with memory address ", overlapped);
    }

    return operation;
}

SocketServiceOperation* SocketServiceOperationContainer::get(LPOVERLAPPED overlapped) const
{
    const auto lock = std::lock_guard<std::mutex>{operationsLock_};
    const auto operationPosition = operations_.find(overlapped);
    if (operationPosition != operations_.cend())
    {
        return operationPosition->second.get();
    }
    return nullptr;
}

}
