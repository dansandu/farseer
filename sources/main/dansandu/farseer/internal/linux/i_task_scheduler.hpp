#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/linux_socket.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"

#include <string>
#include <vector>

namespace dansandu::farseer::internal::linux::i_task_scheduler
{

struct Socket
{
    dansandu::farseer::internal::linux::linux_socket::LinuxSocket socket;
    dansandu::farseer::internal::protocol_reader::ProtocolReader protocolReader;
    SocketIdentifier listeningSocketIdentifier;
    ConnectionCallback connectionCallback;
};

class ITaskScheduler
{
public:
    ITaskScheduler(const ITaskScheduler&) = delete;
    ITaskScheduler(ITaskScheduler&& other) noexcept = delete;
    ITaskScheduler& operator=(const ITaskScheduler&) = delete;
    ITaskScheduler& operator=(ITaskScheduler&& other) noexcept = delete;

    ITaskScheduler() = default;

    virtual ~ITaskScheduler() noexcept
    {
    }

    virtual int getEventPollFileDescriptor() const = 0;

    virtual Socket& insertSocket(const SocketIdentifier socketIdentifier, Socket&& socket) = 0;

    virtual Socket& getSocketOrThrow(const SocketIdentifier socketIdentifier) = 0;

    virtual void eraseSocket(const SocketIdentifier socketIdentifier) = 0;

    virtual SocketIdentifier scheduleListenTask(const std::string& ipAddress, const int port,
                                                ConnectionCallback&& connectionCallback) = 0;

    virtual SocketIdentifier scheduleConnectTask(const std::string& ipAddress, const int port,
                                                 ConnectionCallback&& connectionCallback) = 0;

    virtual void scheduleSendBytesTask(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) = 0;
};

class ITask
{
public:
    ITask(const ITask&) = delete;
    ITask(ITask&& other) noexcept = delete;
    ITask& operator=(const ITask&) = delete;
    ITask& operator=(ITask&& other) noexcept = delete;

    ITask() = default;

    virtual ~ITask() noexcept
    {
    }

    virtual const char* getName() const = 0;

    virtual SocketIdentifier getSocketIdentifier() const = 0;

    virtual void execute(ITaskScheduler& taskScheduler) = 0;
};

}
