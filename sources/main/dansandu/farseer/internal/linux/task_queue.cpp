#if defined(__linux__)
#include "dansandu/farseer/internal/linux/task_queue.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/linux/error.hpp"
#include "dansandu/journey/logging.hpp"

#include <sys/eventfd.h>

#include <iterator>
#include <memory>
#include <mutex>
#include <vector>

using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::linux::error::getLastErrorMessage;
using dansandu::farseer::internal::linux::i_task_scheduler::ITask;

namespace dansandu::farseer::internal::linux::task_queue
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
        WTHROW(InternalSocketError, "Could not create event file descriptor: ", getLastErrorMessage());
    }

    return result;
}

}

TaskQueue::TaskQueue() : eventFileDescriptor_{createEventFileDescriptor()}
{
}

TaskQueue::~TaskQueue() noexcept
{
    ::close(eventFileDescriptor_);
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
        WTHROW(InternalSocketError, "Error writing to event file descriptor ", getLastErrorMessage());
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
