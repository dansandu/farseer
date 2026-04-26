#if defined(__linux__)
#include "dansandu/farseer/internal/linux/task_queue.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/linux/error.hpp"
#include "dansandu/journey/logging.hpp"

#include <sys/eventfd.h>

#include <cstring>
#include <iterator>
#include <memory>
#include <mutex>
#include <vector>

using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::linux::error::getLastErrorMessage;
using dansandu::farseer::internal::linux::event_poll::EventPoll;
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
        LOG_ERROR("Error closing event file descriptor: ", getLastErrorMessage());
    }
}

int createEventFileDescriptor(EventPoll& eventPoll)
{
    const auto initialValue = 0U;
    const auto flags = EFD_NONBLOCK | EFD_CLOEXEC;
    const auto eventFileDescriptor = ::eventfd(initialValue, flags);

    if (eventFileDescriptor == -1)
    {
        WTHROW(InternalSocketError, "Error creating event file descriptor: ", getLastErrorMessage());
    }

    SCOPE_FAILURE([&]() { closeEventOrLog(eventFileDescriptor); });

    eventPoll.subscribe(eventFileDescriptor, EPOLLIN | EPOLLET);

    return eventFileDescriptor;
}

}

TaskQueue::TaskQueue(EventPoll& eventPoll)
    : eventPoll_{eventPoll}, eventFileDescriptor_{createEventFileDescriptor(eventPoll)}
{
}

TaskQueue::~TaskQueue() noexcept
{
    eventPoll_.unsubscribe(eventFileDescriptor_);

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
        WTHROW(InternalSocketError, "Error writing to event file descriptor: ", getLastErrorMessage());
    }

    auto& insertedTask = tasks_.back();

    if (insertedTask)
    {
        LOG_DEBUG("Inserted ", insertedTask->getName(), " with socket ID ", insertedTask->getSocketIdentifier());
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

    uint64_t counter = 0;

    const auto numberOfBytesRead = ::read(eventFileDescriptor_, &counter, sizeof(counter));

    if (numberOfBytesRead == -1)
    {
        WTHROW(InternalSocketError, "Error reading event: ", getLastErrorMessage());
    }

    output.insert(output.end(), std::make_move_iterator(tasks_.begin()), std::make_move_iterator(tasks_.end()));
}

}
#endif
