#if defined(__linux__)
#include "dansandu/farseer/internal/linux/task_scheduler.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/linux/connect_task.hpp"
#include "dansandu/farseer/internal/linux/error.hpp"
#include "dansandu/farseer/internal/linux/linux_socket.hpp"
#include "dansandu/farseer/internal/linux/listen_task.hpp"
#include "dansandu/farseer/internal/linux/send_bytes_task.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/journey/logging.hpp"

#include <string.h>
#include <sys/epoll.h>

using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::linux::connect_task::createConnectTask;
using dansandu::farseer::internal::linux::error::getLastErrorMessage;
using dansandu::farseer::internal::linux::i_task_scheduler::ITask;
using dansandu::farseer::internal::linux::i_task_scheduler::ITaskScheduler;
using dansandu::farseer::internal::linux::i_task_scheduler::Socket;
using dansandu::farseer::internal::linux::linux_socket::SocketType;
using dansandu::farseer::internal::linux::listen_task::createListenTask;
using dansandu::farseer::internal::linux::send_bytes_task::createSendBytesTask;
using dansandu::farseer::internal::linux::task_queue::TaskQueue;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::journey::exception::WideException;

namespace dansandu::farseer::internal::linux::task_scheduler
{

namespace
{

int createEventPollFileDescriptor(const int eventFileDescriptor)
{
    const auto flags = 0;
    const auto result = ::epoll_create1(flags);

    if (result == -1)
    {
        WTHROW(InternalSocketError, "Could not create event poll file descriptor: ", getLastErrorMessage());
    }

    ::epoll_event event;
    ::memset(&event, 0, sizeof(event));

    event.events = EPOLLIN;
    event.data.fd = eventFileDescriptor;

    const auto subscribeResult = ::epoll_ctl(result, EPOLL_CTL_ADD, eventFileDescriptor, &event);

    if (subscribeResult != 0)
    {
        WTHROW(InternalSocketError, "Subscribing event to epoll failed with error ", getLastErrorMessage());
    }

    return result;
}

}

TaskScheduler::TaskScheduler()
    : socketIdentifierSequencer_{invalidSocketIdentifier.getUnderlying() + 1u},
      taskQueue_{},
      eventFileDescriptor_{taskQueue_.getEventFileDescriptor()},
      eventPollFileDescriptor_{createEventPollFileDescriptor(eventFileDescriptor_)},
      thread_{&TaskScheduler::consumeEvents, this}
{
}

TaskScheduler::~TaskScheduler() noexcept
{
    scheduleAbortTask();

    thread_.join();

    const auto event = nullptr;

    const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor_, EPOLL_CTL_DEL, eventFileDescriptor_, event);

    if (subscribeResult == -1)
    {
        LOG_ERROR("Unsubscribing event from event poll failed with error: ", getLastErrorMessage());
    }

    fileDescriptorsToSockets_.clear();

    sockets_.clear();

    ::close(eventPollFileDescriptor_);
}

int TaskScheduler::getEventPollFileDescriptor() const
{
    return eventPollFileDescriptor_;
}

Socket& TaskScheduler::insertSocket(const SocketIdentifier socketIdentifier, Socket&& socket)
{
    const auto [position, inserted] = sockets_.emplace(socketIdentifier, std::move(socket));

    if (!inserted)
    {
        THROW(std::logic_error, "Couldn't insert socket with ID ", socketIdentifier.getUnderlying(),
              " because the ID is used by another socket");
    }

    SCOPE_FAILURE([&] { sockets_.erase(position); });

    const auto [descriptorPosition, descriptorInserted] =
        fileDescriptorsToSockets_.emplace(position->second.socket.getFileDescriptor(), &position->second);

    if (!descriptorInserted)
    {
        THROW(std::logic_error, "Couldn't insert socket with ID ", socketIdentifier.getUnderlying(),
              " because its file descriptor is used by another socket");
    }

    return position->second;
}

Socket& TaskScheduler::getSocketOrThrow(const SocketIdentifier socketIdentifier)
{
    const auto position = sockets_.find(socketIdentifier);

    if (position != sockets_.end())
    {
        return position->second;
    }

    WTHROW(InternalSocketError, "Couldn't find socket with ID ", socketIdentifier.getUnderlying());
}

void TaskScheduler::eraseSocket(const SocketIdentifier socketIdentifier)
{
    const auto position = sockets_.find(socketIdentifier);

    if (position != sockets_.end())
    {
        SCOPE_EXIT(
            [&]
            {
                const auto fileDescriptor = position->second.socket.getFileDescriptor();

                fileDescriptorsToSockets_.erase(fileDescriptor);

                sockets_.erase(position);
            });

        const auto& socket = position->second;

        try
        {
            if (socket.listeningSocketIdentifier != invalidSocketIdentifier)
            {
                const auto listeningSocketPosition = sockets_.find(socket.listeningSocketIdentifier);

                if (listeningSocketPosition != sockets_.end())
                {
                    listeningSocketPosition->second.connectionCallback(SocketEvent::clientClosed, socketIdentifier);
                }
            }
            else
            {
                socket.connectionCallback(SocketEvent::serverClosed, socketIdentifier);
            }

            LOG_INFO("Socket with ID ", socketIdentifier.getUnderlying(), " and address ", socket.socket.getIpAddress(),
                     ':', socket.socket.getPort(), " was closed");
        }
        catch (const WideException& wideException)
        {
            LOG_ERROR("Wide exception was thrown while trying to close socket with message: ",
                      wideException.getMessage());
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR("Exception was thrown while trying to close socket with message: ", exception.what());
        }
    }
}

SocketIdentifier TaskScheduler::scheduleListenTask(const std::string& ipAddress, const int port,
                                                   ConnectionCallback&& connectionCallback)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();
    taskQueue_.insert(createListenTask(socketIdentifier, ipAddress, port, std::move(connectionCallback)));
    return socketIdentifier;
}

SocketIdentifier TaskScheduler::scheduleConnectTask(const std::string& ipAddress, const int port,
                                                    ConnectionCallback&& connectionCallback)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();
    taskQueue_.insert(createConnectTask(socketIdentifier, ipAddress, port, std::move(connectionCallback)));
    return socketIdentifier;
}

void TaskScheduler::scheduleSendBytesTask(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes)
{
    taskQueue_.insert(createSendBytesTask(socketIdentifier, std::move(bytes)));
}

void TaskScheduler::scheduleAbortTask()
{
    taskQueue_.insert(nullptr);
}

void TaskScheduler::handleSocketEventWork(Socket& socket, const uint32_t events)
{
    if (socket.socket.getSocketType() == SocketType::listening)
    {
        while (true)
        {
            auto candidateSocket = socket.socket.accept();

            if (!candidateSocket)
            {
                break;
            }

            const auto acceptedSocketIdentifier = socketIdentifierSequencer_.generate();

            auto& acceptedSocket = insertSocket(
                acceptedSocketIdentifier,
                Socket{
                    .socketIdentifier = acceptedSocketIdentifier,
                    .socket = std::move(*candidateSocket),
                    .protocolReader =
                        ProtocolReader{
                            [&](const SocketIdentifier receivingSocketIdentifier, std::vector<uint8_t>&& response)
                            { scheduleSendBytesTask(receivingSocketIdentifier, std::move(response)); }},
                    .listeningSocketIdentifier = socket.socketIdentifier,
                    .connectionCallback = {},
                });

            LOG_INFO("Accepted client socket ID ", acceptedSocketIdentifier.getUnderlying(), " and address ",
                     acceptedSocket.socket.getIpAddress(), ':', acceptedSocket.socket.getPort());
        }
    }
    else
    {
        if (events & EPOLLOUT)
        {
            LOG_INFO("Connected to socket ID ", socket.socketIdentifier.getUnderlying(), " and address ",
                     socket.socket.getIpAddress(), ':', socket.socket.getPort());
        }

        if (events & EPOLLIN)
        {
            LOG_INFO("Received bytes from socket ID ", socket.socketIdentifier.getUnderlying(), " and address ",
                     socket.socket.getIpAddress(), ':', socket.socket.getPort());

            socket.socket.receive();
        }

        if (events & EPOLLRDHUP)
        {
            LOG_INFO("Connection closed with socket ID ", socket.socketIdentifier.getUnderlying(), " and address ",
                     socket.socket.getIpAddress(), ':', socket.socket.getPort());
        }

        if (events & EPOLLERR)
        {
            LOG_INFO("Connection aborted with socket ID ", socket.socketIdentifier.getUnderlying(), " and address ",
                     socket.socket.getIpAddress(), ':', socket.socket.getPort());
        }
    }
}

void TaskScheduler::handleSocketEvent(const int socketFileDescriptor, const uint32_t events)
{
    const auto socketPosition = fileDescriptorsToSockets_.find(socketFileDescriptor);

    if (socketPosition == fileDescriptorsToSockets_.end())
    {
        LOG_ERROR("Unsubscribing unused socket");

        const auto event = nullptr;

        const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor_, EPOLL_CTL_DEL, socketFileDescriptor, event);

        if (subscribeResult == -1)
        {
            LOG_ERROR("Unsubscribing unused socket to epoll failed with error: ", getLastErrorMessage());
        }

        return;
    }

    const auto socketIdentifier = socketPosition->second->socketIdentifier.getUnderlying();

    LOG_DEBUG("Processing events for socket with ID ", socketIdentifier);

    try
    {
        handleSocketEventWork(*(socketPosition->second), events);
    }
    catch (const WideException& exception)
    {
        LOG_ERROR("Processing events for socket with ID ", socketIdentifier,
                  "failed with wide exception: ", exception.getMessage());
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR("Processing events for socket with ID ", socketIdentifier,
                  "failed with exception: ", exception.what());
    }
}

namespace
{

bool consumeTasks(ITaskScheduler& taskScheduler, TaskQueue& taskQueue, std::vector<std::unique_ptr<ITask>>& tasksBuffer)
{
    taskQueue.transfer(tasksBuffer);

    SCOPE_EXIT([&] { tasksBuffer.clear(); });

    for (auto& task : tasksBuffer)
    {
        if (task == nullptr)
        {
            return true;
        }

        const auto taskSocketIdentifier = task->getSocketIdentifier().getUnderlying();
        const auto taskName = task->getName();

        LOG_DEBUG("Executing ", taskName, " with socket ID ", taskSocketIdentifier);

        try
        {
            task->execute(taskScheduler);
        }
        catch (const WideException& exception)
        {
            LOG_ERROR(taskName, " with socket ID ", taskSocketIdentifier,
                      " execution failed with wide exception: ", exception.getMessage());
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR(taskName, " with socket ID ", taskSocketIdentifier,
                      " execution failed with exception: ", exception.what());
        }
    }

    return false;
}

}

void TaskScheduler::consumeEventsWork()
{
    auto events = std::vector<epoll_event>{};

    // preserve capacity to avoid reallocation
    auto tasksBuffer = std::vector<std::unique_ptr<ITask>>{};

    while (true)
    {
        const auto maximumNumberOfEvents = 1024;

        // add one event for the task queue event file descriptor
        const auto numberOfEvents = std::max(static_cast<int>(sockets_.size()) + 1, maximumNumberOfEvents);

        events.resize(numberOfEvents);

        const auto timeout = -1;
        const auto numberOfPendingEvents =
            ::epoll_wait(eventPollFileDescriptor_, events.data(), static_cast<int>(events.size()), timeout);

        if (numberOfPendingEvents == -1)
        {
            WTHROW(InternalSocketError, "Couldn't wait on events due to error: ", getLastErrorMessage());
        }

        if (const auto abort = consumeTasks(*this, taskQueue_, tasksBuffer); abort)
        {
            LOG_DEBUG("Received abort task");
            return;
        }

        for (auto index = 0; index < numberOfPendingEvents; ++index)
        {
            if (events[index].data.fd == eventFileDescriptor_)
            {
                uint64_t counter = 0;

                const auto numberOfBytesRead = ::read(eventFileDescriptor_, &counter, sizeof(counter));

                if (numberOfBytesRead == -1)
                {
                    WTHROW(InternalSocketError, "Reading event failed with error ", getLastErrorMessage());
                }
            }
            else
            {
                handleSocketEvent(events[index].data.fd, events[index].events);
            }
        }

        events.clear();
    }
}

void TaskScheduler::consumeEvents()
{
    LOG_DEBUG("Started events consumer thread");

    try
    {
        consumeEventsWork();

        LOG_DEBUG("Gracefully exited events consumer thread");
    }
    catch (const WideException& exception)
    {
        LOG_CRITICAL("Events consumer thread exited with wide exception: ", exception.getMessage());
    }
    catch (const std::exception& exception)
    {
        LOG_CRITICAL("Events consumer thread exited with exception: ", exception.what());
    }
}

}
#endif
