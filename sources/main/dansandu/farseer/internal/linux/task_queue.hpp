#pragma once

#include "dansandu/farseer/internal/linux/i_task_scheduler.hpp"

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

    TaskQueue();

    ~TaskQueue() noexcept;

    int getEventFileDescriptor() const;

    void insert(std::unique_ptr<dansandu::farseer::internal::linux::i_task_scheduler::ITask>&& task);

    void transfer(std::vector<std::unique_ptr<dansandu::farseer::internal::linux::i_task_scheduler::ITask>>& output);

private:
    const int eventFileDescriptor_;
    std::vector<std::unique_ptr<dansandu::farseer::internal::linux::i_task_scheduler::ITask>> tasks_;
    std::mutex mutex_;
};

}
