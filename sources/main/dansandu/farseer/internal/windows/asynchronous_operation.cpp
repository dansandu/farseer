#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/ballotin/string.hpp"
#include "dansandu/farseer/internal/windows/accept_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/close_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/connect_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/farseer/internal/windows/listen_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/receive_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/register_message_consumer_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/send_bytes_asynchronous_operation.hpp"

using dansandu::ballotin::string::toWideString;
using dansandu::farseer::exception::InternalSocketServiceException;
using dansandu::farseer::internal::windows::error::getLastErrorMessage;

namespace dansandu::farseer::internal::windows::asynchronous_operation
{

namespace
{

HANDLE initializeIoCompletionPort()
{
    const auto handle = HANDLE{INVALID_HANDLE_VALUE};
    const auto existingCompletionPort = HANDLE{nullptr};
    const auto numberOfConcurrentThreads = DWORD{0};

    const auto completionPort =
        ::CreateIoCompletionPort(handle, existingCompletionPort, initialCompletionKey, numberOfConcurrentThreads);

    if (completionPort != nullptr)
    {
        return completionPort;
    }

    THROW(std::runtime_error, "Couldn't create I/O completion port queue: ", getLastErrorMessage());
}

}

AsynchronousOperationContainer::AsynchronousOperationContainer()
    : completionPort_{initializeIoCompletionPort()}, serviceIdSequencer_{initialCompletionKey + 1}
{
}

AsynchronousOperationContainer::~AsynchronousOperationContainer() noexcept
{
    ::CloseHandle(completionPort_);
}

SocketServiceId AsynchronousOperationContainer::insertOperation(std::unique_ptr<AsynchronousOperation> operation)
{
    const auto overlapped = operation->getOverlapped();
    const auto name = operation->getName();
    const auto serviceId = operation->getServiceId();

    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto [position, inserted] = operations_.insert({overlapped, std::move(operation)});

    if (!inserted)
    {
        THROW(std::logic_error, "Couldn't push ", name, " with service ID ", serviceId.getInteger(),
              " because operation with address ", overlapped, " already exists");
    }

    SCOPE_FAILURE([&]() { operations_.erase(position); });

    position->second->postToCompletionPort(socketServiceContainer_, completionPort_);

    LOG_DEBUG("Inserted ", name, " with service ID ", serviceId.getInteger(), " and address ", overlapped);

    return serviceId;
}

SocketServiceId
AsynchronousOperationContainer::createConnectAsynchronousOperation(const std::wstring& ipAddress, const int port,
                                                                   ConnectionCallbackType connectionCallback)
{
    return insertOperation(
        dansandu::farseer::internal::windows::connect_asynchronous_operation::createConnectAsynchronousOperation(
            serviceIdSequencer_, ipAddress, port, std::move(connectionCallback)));
}

SocketServiceId
AsynchronousOperationContainer::createListenAsynchronousOperation(const std::wstring& ipAddress, const int port,
                                                                  ConnectionCallbackType connectionCallback)
{
    return insertOperation(
        dansandu::farseer::internal::windows::listen_asynchronous_operation::createListenAsynchronousOperation(
            serviceIdSequencer_, ipAddress, port, std::move(connectionCallback)));
}

void AsynchronousOperationContainer::createAcceptAsynchronousOperation(const SocketServiceId listeningServiceId)
{
    insertOperation(
        dansandu::farseer::internal::windows::accept_asynchronous_operation::createAcceptAsynchronousOperation(
            serviceIdSequencer_, listeningServiceId));
}

void AsynchronousOperationContainer::createReceiveAsynchronousOperation(const SocketServiceId serviceId)
{
    insertOperation(
        dansandu::farseer::internal::windows::receive_asynchronous_operation::createReceiveAsynchronousOperation(
            serviceId));
}

void AsynchronousOperationContainer::createRegisterMessageConsumerAsynchronousOperation(
    const SocketServiceId serviceId, const ProtocolIdentifier protocolIdentifier,
    std::function<void(std::any)> messageConsumer)
{
    insertOperation(dansandu::farseer::internal::windows::register_message_consumer_asynchronous_operation::
                        createRegisterMessageConsumerAsynchronousOperation(serviceId, protocolIdentifier,
                                                                           std::move(messageConsumer)));
}

void AsynchronousOperationContainer::createSendBytesAsynchronousOperation(const SocketServiceId serviceId,
                                                                          std::vector<uint8_t> bytes)
{
    insertOperation(
        dansandu::farseer::internal::windows::send_bytes_asynchronous_operation::createSendBytesAsynchronousOperation(
            serviceId, std::move(bytes)));
}

void AsynchronousOperationContainer::createCloseAsynchronousOperation(const SocketServiceId serviceId)
{
    insertOperation(
        dansandu::farseer::internal::windows::close_asynchronous_operation::createCloseAsynchronousOperation(
            serviceId));
}

void AsynchronousOperationContainer::createAbortAsynchronousOperation()
{
    const auto numberOfBytesTransferred = 0;
    const auto overlapped = LPOVERLAPPED{nullptr};
    const auto postResult =
        ::PostQueuedCompletionStatus(completionPort_, numberOfBytesTransferred, initialCompletionKey, overlapped);

    if (postResult == 0)
    {
        LOG_ERROR("Couldn't post abort operation to queue: ", getLastErrorMessage());
    }
    else
    {
        LOG_DEBUG("Posted abort operation to queue");
    }
}

bool AsynchronousOperationContainer::waitAndConsumeAsynchronousOperation()
{
    auto numberOfBytesTransferred = DWORD{0};
    auto completionKey = 0ULL;
    auto overlapped = LPOVERLAPPED{nullptr};
    auto timeout = INFINITE;

    const auto dequeueResult =
        ::GetQueuedCompletionStatus(completionPort_, &numberOfBytesTransferred, &completionKey, &overlapped, timeout);

    try
    {
        if (dequeueResult == TRUE)
        {
            if (completionKey == initialCompletionKey && overlapped == nullptr)
            {
                LOG_DEBUG("Received abort operation");

                return false;
            }

            handleSuccessfulAsynchronousOperation(overlapped, numberOfBytesTransferred);
        }
        else
        {
            if (overlapped == nullptr)
            {
                LOG_ERROR("Could not dequeue operation from completion queue");

                return false;
            }

            handleFailedAsynchronousOperation(overlapped, toWideString(getLastErrorMessage()));
        }
    }
    catch (const InternalSocketServiceException& exception)
    {
        handleFailedAsynchronousOperation(overlapped, exception.getMessage());
    }
    catch (const std::exception& exception)
    {
        handleFailedAsynchronousOperation(overlapped, toWideString(exception.what()));
    }

    return true;
}

void AsynchronousOperationContainer::handleSuccessfulAsynchronousOperation(const LPWSAOVERLAPPED overlapped,
                                                                           const DWORD numberOfBytesTransferred)
{
    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto position = operations_.find(overlapped);

    if (position != operations_.cend())
    {
        const auto name = position->second->getName();
        const auto serviceId = position->second->getServiceId();

        LOG_DEBUG("Executing ", name, " with service ID ", serviceId.getInteger());

        const auto pop = position->second->finalize(serviceIdSequencer_, socketServiceContainer_, *this,
                                                    completionPort_, numberOfBytesTransferred);

        if (pop)
        {
            operations_.erase(position);

            LOG_DEBUG("Erased ", name, " with service ID ", serviceId.getInteger());
        }
        else
        {
            LOG_DEBUG("Keeping ", name, " with service ID ", serviceId.getInteger());
        }
    }
    else
    {
        THROW(std::logic_error, "Couldn't execute unknown operation with address ", overlapped);
    }
}

void AsynchronousOperationContainer::handleFailedAsynchronousOperation(const LPWSAOVERLAPPED overlapped,
                                                                       const std::wstring_view message)
{
    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto position = operations_.find(overlapped);

    if (position != operations_.cend())
    {
        const auto name = position->second->getName();
        const auto serviceId = position->second->getServiceId().getInteger();

        if (message.empty())
        {
            LOG_ERROR(name, " with service ID ", serviceId, " and address ", overlapped, " failed");
        }
        else
        {
            LOG_ERROR(name, " with service ID ", serviceId, " and address ", overlapped, " failed: ", message);
        }

        operations_.erase(position);
    }
    else
    {
        if (message.empty())
        {
            LOG_ERROR("Unknown operation with address ", overlapped, " failed");
        }
        else
        {
            LOG_ERROR("Unknown operation with address ", overlapped, " failed: ", message);
        }
    }
}

}
