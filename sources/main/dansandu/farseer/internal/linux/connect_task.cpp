#if defined(__linux__)
#include "dansandu/farseer/internal/linux/connect_task.hpp"
#include "dansandu/farseer/common.hpp"

using dansandu::farseer::internal::linux::socket_container::SocketContainer;
using dansandu::farseer::internal::linux::task::ITask;

namespace dansandu::farseer::internal::linux::connect_task
{

class ConnectTask : public ITask
{
public:
    ConnectTask(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback)
        : socketIdentifier_{socketIdentifier},
          ipAddress_{ipAddress},
          port_{port},
          connectionCallback_{std::move(connectionCallback)}
    {
    }

    const char* getName() const override
    {
        return "ConnectTask";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    void execute(SocketContainer& socketContainer) override
    {
        socketContainer.connect(socketIdentifier_, ipAddress_, port_, std::move(connectionCallback_));
    }

private:
    const SocketIdentifier socketIdentifier_;
    const std::string ipAddress_;
    const int port_;
    UniqueFunction<void(const SocketEvent, const SocketIdentifier)> connectionCallback_;
};

std::unique_ptr<ITask>
createConnectTask(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                  UniqueFunction<void(const SocketEvent, const SocketIdentifier)>&& connectionCallback)
{
    return std::make_unique<ConnectTask>(socketIdentifier, ipAddress, port, std::move(connectionCallback));
}

}
#endif
