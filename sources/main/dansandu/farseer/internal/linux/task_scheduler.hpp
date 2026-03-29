#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/i_task_scheduler.hpp"
#include "dansandu/farseer/internal/linux/task_queue.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace dansandu::farseer::internal::linux::task_scheduler
{

class TaskScheduler : public dansandu::farseer::internal::linux::i_task_scheduler::ITaskScheduler
{
public:
    TaskScheduler();

    ~TaskScheduler() noexcept;

    int getEventPollFileDescriptor() const override;

    dansandu::farseer::internal::linux::i_task_scheduler::Socket&
    insertSocket(const SocketIdentifier socketIdentifier,
                 dansandu::farseer::internal::linux::i_task_scheduler::Socket&& socket) override;

    dansandu::farseer::internal::linux::i_task_scheduler::Socket&
    getSocketOrThrow(const SocketIdentifier socketIdentifier) override;

    void eraseSocket(const SocketIdentifier socketIdentifier) override;

    SocketIdentifier scheduleListenTask(const std::string& ipAddress, const int port,
                                        ConnectionCallback&& connectionCallback) override;

    SocketIdentifier scheduleConnectTask(const std::string& ipAddress, const int port,
                                         ConnectionCallback&& connectionCallback) override;

    void scheduleSendBytesTask(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) override;

private:
    void scheduleAbortTask();

    void handleSocketEvent(const int socketFileDescriptor);

    void consumeEventsWork();

    void consumeEvents();

    dansandu::farseer::internal::sequencer::Sequencer<SocketIdentifier> socketIdentifierSequencer_;
    std::map<SocketIdentifier, dansandu::farseer::internal::linux::i_task_scheduler::Socket> sockets_;
    std::map<int, dansandu::farseer::internal::linux::i_task_scheduler::Socket*> fileDescriptorsToSockets_;
    dansandu::farseer::internal::linux::task_queue::TaskQueue taskQueue_;
    const int eventFileDescriptor_;
    const int eventPollFileDescriptor_;
    std::thread thread_;
};

}
