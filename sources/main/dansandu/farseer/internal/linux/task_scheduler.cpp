#if defined(__linux__)
#include "dansandu/farseer/internal/linux/task_scheduler.hpp"
#include "dansandu/farseer/internal/linux/connect_task.hpp"

#include <sys/epoll.h>
#include <sys/eventfd.h>

using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::linux::connect_task::createConnectTask;
using dansandu::journey::exception::WideException;

namespace dansandu::farseer::internal::linux::task_scheduler
{

namespace
{

int createEventFileDescriptor()
{
    const auto initialValue = 0U;
    const auto flags = 0;
    const auto result = ::eventfd(initialValue, flags);

    if (result == -1)
    {
        WTHROW(InternalSocketError, "Could not create event file descriptor: ", ::strerror(::errno));
    }

    return result;
}

int createEventPollFileDescriptor(const int eventFileDescriptor)
{
    const auto flags = 0;
    const auto result = ::epoll_create1(flags);

    if (result == -1)
    {
        WTHROW(InternalSocketError, "Could not create event poll file descriptor: ", ::strerror(::errno));
    }

    ::epoll_event event;
    std::memset(&event, 0, sizeof(event));

    event.events = EPOLLIN;
    event.data.fd = socket_;

    const auto subscribeResult = ::epoll_ctl(result, EPOLL_CTL_ADD, eventFileDescriptor, &event);

    if (subscribeResult != 0)
    {
        WTHROW(InternalSocketError, "Subscribing event to epoll failed with error ", ::strerror(::errno));
    }

    return result;
}

}

TaskScheduler::TaskScheduler()
    : eventFileDescriptor_{createEventFileDescriptor()},
      eventPollFileDescriptor_{createEventPollFileDescriptor(eventFileDescriptor_)},
      thread_{&TaskScheduler::consumeEvents, this}
{
}

TaskScheduler::~TaskScheduler() noexcept
{
    scheduleAbortTask();

    thread_.join();

    sockets_.clear();

    ::close(eventFileDescriptor_);

    ::close(eventPollFileDescriptor_);
}

int TaskScheduler::getEventPollFileDescriptor()
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
        SCOPE_EXIT([&] { sockets_.erase(position); });

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

        LOG_INFO("Socket with ID ", socketIdentifier.getUnderlying(), " and address ", socket.socket.getIpAddress(),
                 ':', socket.socket.getPort(), " was closed");
    }
}

SocketIdentifier TaskScheduler::scheduleConnectTask(const std::string& ipAddress, const int port,
                                                    ConnectionCallback&& connectionCallback)
{
    const auto socketIdentifier = socketIdentifierSequencer_.generate();
    insertTask(createConnectTask(socketIdentifier, ipAddress, port, std::move(connectionCallback)));
    return socketIdentifier;
}

void TaskScheduler::scheduleSendBytesTask(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes)
{
    insertTask(createSendBytesTask(socketIdentifier, std::move(bytes)));
}

void TaskScheduler::consumeEvents()
{
    LOG_DEBUG("Started events consumer thread");

    while (true)
    {
        break;
    }

    LOG_DEBUG("Exiting events consumer thread");
}

void TaskScheduler::insertTask(std::unique_ptr<ITask>&& task)
{
    const auto socketIdentifier = task->getSocketIdentifier();

    const auto name = task->getName();

    const auto lock = std::lock_guard<std::recursive_mutex>{tasksMutex_};

    tasks_.push(std::move(task));

    const uint64_t increment = 1;

    const auto result = ::write(eventFileDescriptor_, &increment, sizeof(increment));

    if (result == -1)
    {
        WTHROW(InternalSocketError, "Error writing to event file descriptor ", ::strerror(::errno));
    }

    LOG_DEBUG("Inserted ", name, " with socket ID ", socketIdentifier.getUnderlying());
}

}
#endif
