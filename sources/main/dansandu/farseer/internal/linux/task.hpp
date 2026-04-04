#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/socket_container.hpp"

namespace dansandu::farseer::internal::linux::task
{

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

    virtual void execute(dansandu::farseer::internal::linux::socket_container::SocketContainer& socketContainer) = 0;
};

}
