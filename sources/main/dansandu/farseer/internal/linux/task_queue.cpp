#if defined(__linux__)
#include "dansandu/farseer/internal/linux/task_queue.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/linux/error.hpp"
#include "dansandu/journey/logging.hpp"

#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <cstring>
#include <iterator>
#include <memory>
#include <mutex>
#include <vector>

using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::linux::error::getLastErrorMessage;
using dansandu::farseer::internal::linux::task::ITask;

namespace dansandu::farseer::internal::linux::task_queue
{

namespace
{

void closeEventOrLog(const int eventFileDescriptor)
{
    const auto closeResult = ::close(eventFileDescriptor);

    if (closeResult == -1)
    {
        LOG_ERROR("Error closing event: ", getLastErrorMessage());
    }
}

int createEventFileDescriptor(const int eventPollFileDescriptor)
{
    const auto initialValue = 0U;
    const auto flags = 0;
    const auto eventFileDescriptor = ::eventfd(initialValue, flags);

    if (eventFileDescriptor == -1)
    {
        WTHROW(InternalSocketError, "Error creating event: ", getLastErrorMessage());
    }

    SCOPE_FAILURE([&]() { closeEventOrLog(eventFileDescriptor); });

    ::epoll_event event;

    std::memset(&event, 0, sizeof(event));

    event.events = EPOLLIN | EPOLLET;
    event.data.fd = eventFileDescriptor;

    const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor, EPOLL_CTL_ADD, eventFileDescriptor, &event);

    if (subscribeResult != 0)
    {
        WTHROW(InternalSocketError, "Error subscribing event to event poll: ", getLastErrorMessage());
    }

    return eventFileDescriptor;
}

}

TaskQueue::TaskQueue(const int eventPollFileDescriptor)
    : eventPollFileDescriptor_{eventPollFileDescriptor},
      eventFileDescriptor_{createEventFileDescriptor(eventPollFileDescriptor)}
{
}

TaskQueue::~TaskQueue() noexcept
{
    const auto event = nullptr;

    const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor_, EPOLL_CTL_DEL, eventFileDescriptor_, event);

    if (subscribeResult == -1)
    {
        LOG_ERROR("Error unsubscribing event from event poll: ", getLastErrorMessage());
    }

    closeEventOrLog(eventFileDescriptor_);
}

int TaskQueue::getEventFileDescriptor() const
{
    return eventFileDescriptor_;
}

void TaskQueue::insert(std::unique_ptr<ITask>&& task)
{
    const auto lock = std::lock_guard<std::mutex>{mutex_};
    tasks_.push_back(std::move(task));

    SCOPE_FAILURE([&]() { tasks_.pop_back(); });

    const uint64_t increment = 1;

    const auto writeResult = ::write(eventFileDescriptor_, &increment, sizeof(increment));

    if (writeResult == -1)
    {
        WTHROW(InternalSocketError, "Error writing event: ", getLastErrorMessage());
    }

    auto& insertedTask = tasks_.back();

    if (insertedTask)
    {
        LOG_DEBUG("Inserted ", insertedTask->getName(), " with socket ID ",
                  insertedTask->getSocketIdentifier().getUnderlying());
    }
    else
    {
        LOG_DEBUG("Inserted abort task");
    }
}

void TaskQueue::transfer(std::vector<std::unique_ptr<ITask>>& output)
{
    const auto lock = std::lock_guard<std::mutex>{mutex_};

    SCOPE_SUCCESS([&]() { tasks_.clear(); });

    output.insert(output.end(), std::make_move_iterator(tasks_.begin()), std::make_move_iterator(tasks_.end()));
}

}
#endif
