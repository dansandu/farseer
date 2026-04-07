#if defined(__linux__)
#include "dansandu/farseer/internal/linux/close_task.hpp"
#include "dansandu/farseer/common.hpp"

using dansandu::farseer::internal::linux::socket_container::SocketContainer;
using dansandu::farseer::internal::linux::task::ITask;

namespace dansandu::farseer::internal::linux::close_task
{

class CloseTask : public ITask
{
public:
    CloseTask(const SocketIdentifier socketIdentifier) : socketIdentifier_{socketIdentifier}
    {
    }

    const char* getName() const override
    {
        return "CloseTask";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    void execute(SocketContainer& socketContainer) override
    {
        socketContainer.eraseSocket(socketIdentifier_);
    }

private:
    const SocketIdentifier socketIdentifier_;
};

std::unique_ptr<ITask> createCloseTask(const SocketIdentifier socketIdentifier)
{
    return std::make_unique<CloseTask>(socketIdentifier);
}

}
#endif
