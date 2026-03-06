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
#include "dansandu/farseer/internal/windows/register_request_callback_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/send_bytes_asynchronous_operation.hpp"
#include "dansandu/farseer/internal/windows/send_request_asynchronous_operation.hpp"

using dansandu::ballotin::string::toWideString;
using dansandu::farseer::exception::InternalSocketServiceException;
using dansandu::farseer::internal::windows::error::getErrorMessageFromCode;
using dansandu::farseer::internal::windows::error::getLastErrorCode;
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

AsynchronousOperationScheduler::AsynchronousOperationScheduler()
    : completionPort_{initializeIoCompletionPort()}, serviceIdSequencer_{initialCompletionKey + 1}
{
}

AsynchronousOperationScheduler::~AsynchronousOperationScheduler() noexcept
{
    ::CloseHandle(completionPort_);
}

SocketServiceId AsynchronousOperationScheduler::insertOperation(std::unique_ptr<AsynchronousOperation> operation)
{
    const auto overlapped = operation->getOverlapped();
    const auto name = operation->getName();
    const auto serviceId = operation->getServiceId();

    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto [position, inserted] = operations_.insert({overlapped, std::move(operation)});

    if (!inserted)
    {
        THROW(std::logic_error, "Couldn't push ", name, " with service ID ", serviceId.getUnderlying(),
              " because operation already exists");
    }

    SCOPE_FAILURE([&]() { operations_.erase(position); });

    position->second->postToCompletionPort(socketServiceContainer_, *this, completionPort_);

    LOG_DEBUG("Inserted ", name, " with service ID ", serviceId.getUnderlying());

    return serviceId;
}

SocketServiceId
AsynchronousOperationScheduler::createConnectAsynchronousOperation(const std::wstring& ipAddress, const int port,
                                                                   ConnectionCallbackType&& connectionCallback)
{
    return insertOperation(
        dansandu::farseer::internal::windows::connect_asynchronous_operation::createConnectAsynchronousOperation(
            serviceIdSequencer_, ipAddress, port, std::move(connectionCallback)));
}

SocketServiceId
AsynchronousOperationScheduler::createListenAsynchronousOperation(const std::wstring& ipAddress, const int port,
                                                                  ConnectionCallbackType&& connectionCallback)
{
    return insertOperation(
        dansandu::farseer::internal::windows::listen_asynchronous_operation::createListenAsynchronousOperation(
            serviceIdSequencer_, ipAddress, port, std::move(connectionCallback)));
}

void AsynchronousOperationScheduler::createAcceptAsynchronousOperation(const SocketServiceId listeningServiceId)
{
    insertOperation(
        dansandu::farseer::internal::windows::accept_asynchronous_operation::createAcceptAsynchronousOperation(
            serviceIdSequencer_, listeningServiceId));
}

void AsynchronousOperationScheduler::createReceiveAsynchronousOperation(const SocketServiceId serviceId)
{
    insertOperation(
        dansandu::farseer::internal::windows::receive_asynchronous_operation::createReceiveAsynchronousOperation(
            serviceId));
}

void AsynchronousOperationScheduler::createRegisterMessageConsumerAsynchronousOperation(
    const SocketServiceId serviceId, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<void(std::any&&)>&& messageConsumer)
{
    insertOperation(dansandu::farseer::internal::windows::register_message_consumer_asynchronous_operation::
                        createRegisterMessageConsumerAsynchronousOperation(serviceId, protocolIdentifier,
                                                                           std::move(messageConsumer)));
}

void AsynchronousOperationScheduler::createRegisterRequestCallbackAsynchronousOperation(
    const SocketServiceId serviceId, const ProtocolIdentifier protocolIdentifier,
    UniqueFunction<std::any(std::any&&)>&& requestConsumer)
{
    insertOperation(dansandu::farseer::internal::windows::register_request_callback_asynchronous_operation::
                        createRegisterRequestCallbackAsynchronousOperation(serviceId, protocolIdentifier,
                                                                           std::move(requestConsumer)));
}

void AsynchronousOperationScheduler::createSendBytesAsynchronousOperation(const SocketServiceId serviceId,
                                                                          std::vector<uint8_t>&& bytes)
{
    insertOperation(
        dansandu::farseer::internal::windows::send_bytes_asynchronous_operation::createSendBytesAsynchronousOperation(
            serviceId, std::move(bytes)));
}

void AsynchronousOperationScheduler::createSendRequestAsynchronousOperation(
    const SocketServiceId serviceId, const ProtocolSequenceNumber sequenceNumber, std::vector<uint8_t>&& bytes,
    UniqueFunction<void(std::any&&)>&& expectedResponseConsumer)
{
    insertOperation(dansandu::farseer::internal::windows::send_request_asynchronous_operation::
                        createSendRequestAsynchronousOperation(serviceId, sequenceNumber, std::move(bytes),
                                                               std::move(expectedResponseConsumer)));
}

void AsynchronousOperationScheduler::createCloseAsynchronousOperation(const SocketServiceId serviceId)
{
    insertOperation(
        dansandu::farseer::internal::windows::close_asynchronous_operation::createCloseAsynchronousOperation(
            serviceId));
}

void AsynchronousOperationScheduler::createAbortAsynchronousOperation()
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

bool AsynchronousOperationScheduler::waitAndConsumeAsynchronousOperation()
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
            const auto errorCode = getLastErrorCode();

            if (overlapped == nullptr)
            {
                const auto errorMessage = getErrorMessageFromCode(errorCode);

                LOG_ERROR("Could not dequeue operation from completion queue: ", errorMessage);

                return false;
            }

            handleFailedAsynchronousOperation(overlapped, errorCode);
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

void AsynchronousOperationScheduler::handleSuccessfulAsynchronousOperation(const LPWSAOVERLAPPED overlapped,
                                                                           const DWORD numberOfBytesTransferred)
{
    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto position = operations_.find(overlapped);

    if (position != operations_.cend())
    {
        const auto name = position->second->getName();
        const auto serviceId = position->second->getServiceId();

        LOG_DEBUG("Executing ", name, " with service ID ", serviceId.getUnderlying());

        const auto pop = position->second->finalize(serviceIdSequencer_, socketServiceContainer_, *this,
                                                    completionPort_, numberOfBytesTransferred);

        if (pop)
        {
            operations_.erase(position);

            LOG_DEBUG("Erased ", name, " with service ID ", serviceId.getUnderlying());
        }
        else
        {
            LOG_DEBUG("Keeping ", name, " with service ID ", serviceId.getUnderlying());
        }
    }
    else
    {
        THROW(std::logic_error, "Couldn't execute unknown operation");
    }
}

void AsynchronousOperationScheduler::handleFailedAsynchronousOperation(const LPWSAOVERLAPPED overlapped,
                                                                       const DWORD errorCode)
{
    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto position = operations_.find(overlapped);

    if (position != operations_.cend())
    {
        SCOPE_EXIT([&] { operations_.erase(position); });

        const auto name = position->second->getName();
        const auto serviceId = position->second->getServiceId().getUnderlying();
        const auto level = position->second->reinterpretSystemErrorCode(errorCode);
        const auto message = getErrorMessageFromCode(errorCode);

        LOG(level, name, " with service ID ", serviceId, " failed: ", message);
    }
    else
    {
        const auto errorMessage = getErrorMessageFromCode(errorCode);

        LOG_ERROR("Unknown operation failed: ", errorMessage);
    }
}

void AsynchronousOperationScheduler::handleFailedAsynchronousOperation(const LPWSAOVERLAPPED overlapped,
                                                                       const std::wstring_view message)
{
    const auto lock = std::lock_guard<std::recursive_mutex>{operationsMutex_};
    const auto position = operations_.find(overlapped);

    if (position != operations_.cend())
    {
        SCOPE_EXIT([&] { operations_.erase(position); });

        const auto name = position->second->getName();
        const auto serviceId = position->second->getServiceId().getUnderlying();

        if (message.empty())
        {
            LOG_ERROR(name, " with service ID ", serviceId, " failed");
        }
        else
        {
            LOG_ERROR(name, " with service ID ", serviceId, " failed: ", message);
        }
    }
    else
    {
        if (message.empty())
        {
            LOG_ERROR("Unknown operation failed");
        }
        else
        {
            LOG_ERROR("Unknown operation failed: ", message);
        }
    }
}

}
