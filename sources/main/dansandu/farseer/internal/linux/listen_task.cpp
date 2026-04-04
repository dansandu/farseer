#if defined(__linux__)
#include "dansandu/farseer/internal/linux/listen_task.hpp"
#include "dansandu/farseer/common.hpp"

using dansandu::farseer::internal::linux::socket_container::SocketContainer;
using dansandu::farseer::internal::linux::task::ITask;

namespace dansandu::farseer::internal::linux::listen_task
{

class ListenTask : public ITask
{
public:
    ListenTask(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
               ConnectionCallback&& connectionCallback)
        : socketIdentifier_{socketIdentifier},
          ipAddress_{ipAddress},
          port_{port},
          connectionCallback_{std::move(connectionCallback)}
    {
    }

    const char* getName() const override
    {
        return "ListenTask";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    void execute(SocketContainer& socketContainer) override
    {
        socketContainer.listen(socketIdentifier_, ipAddress_, port_, std::move(connectionCallback_));
    }

private:
    const SocketIdentifier socketIdentifier_;
    const std::string ipAddress_;
    const int port_;
    ConnectionCallback connectionCallback_;
};

std::unique_ptr<ITask> createListenTask(const SocketIdentifier socketIdentifier, const std::string& ipAddress,
                                        const int port, ConnectionCallback&& connectionCallback)
{
    return std::make_unique<ListenTask>(socketIdentifier, ipAddress, port, std::move(connectionCallback));
}

}
#endif
