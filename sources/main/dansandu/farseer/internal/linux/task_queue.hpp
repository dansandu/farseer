#pragma once

#include "dansandu/farseer/internal/linux/event_poll.hpp"
#include "dansandu/farseer/internal/linux/task.hpp"

#include <memory>
#include <mutex>
#include <vector>

namespace dansandu::farseer::internal::linux::task_queue
{

class TaskQueue
{
public:
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue(TaskQueue&&) noexcept = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;
    TaskQueue& operator=(TaskQueue&&) noexcept = delete;

    explicit TaskQueue(dansandu::farseer::internal::linux::event_poll::EventPoll& eventPoll);

    ~TaskQueue() noexcept;

    int getEventFileDescriptor() const;

    void insert(std::unique_ptr<dansandu::farseer::internal::linux::task::ITask>&& task);

    void transfer(std::vector<std::unique_ptr<dansandu::farseer::internal::linux::task::ITask>>& output);

private:
    dansandu::farseer::internal::linux::event_poll::EventPoll& eventPoll_;
    const int eventFileDescriptor_;
    std::vector<std::unique_ptr<dansandu::farseer::internal::linux::task::ITask>> tasks_;
    std::mutex mutex_;
};

}
