#if defined(__linux__)
#include "dansandu/farseer/internal/linux/connect_task.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/linux_socket.hpp"
#include "dansandu/farseer/internal/linux/task_scheduler.hpp"

using dansandu::farseer::internal::linux::linux_socket::LinuxSocket;
using dansandu::farseer::internal::linux::task_scheduler::ITask;
using dansandu::farseer::internal::linux::task_scheduler::ITaskScheduler;
using dansandu::farseer::internal::linux::task_scheduler::Socket;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;

namespace dansandu::farseer::internal::linux::connect_task
{

class ConnectTask : public ITask
{
public:
    ConnectTask(const SocketIdentifier socketIdentifier, const std::string& ipAddress, const int port,
                ConnectionCallback&& connectionCallback)
        : socketIdentifier_{socketIdentifier},
          ipAddress_{ipAddress},
          port_{port},
          connectionCallback_{std::move(connectionCallback)}
    {
    }

    const char* getName() override
    {
        return "ConnectTask";
    }

    SocketIdentifier getSocketIdentifier() override
    {
        return socketIdentifier_;
    }

    void execute(ITaskScheduler& taskScheduler) override
    {
        auto tempSocket = LinuxSocket{};

        tempSocket.connect(ipAddress_, port_, taskScheduler.getEventPollFileDescriptor());

        taskScheduler.insertSocket(
            socketIdentifier_,
            Socket{
                .socket = std::move(tempSocket),
                .protocolReader =
                    ProtocolReader{
                        [&](const SocketIdentifier receivingSocketIdentifier, std::vector<uint8_t>&& response)
                        { taskScheduler.scheduleSendBytesTask(receivingSocketIdentifier, std::move(response)); }},
                .listeningSocketIdentifier = invalidSocketIdentifier,
                .connectionCallback = std::move(connectionCallback_),
            });
    }

private:
    const SocketIdentifier socketIdentifier_;
    const std::string ipAddress_;
    const int port_;
    ConnectionCallback connectionCallback_;
};

std::unique_ptr<ITask> createConnectTask(const SocketIdentifier socketIdentifier, const std::string& ipAddress,
                                         const int port, ConnectionCallback&& connectionCallback)
{
    return std::make_unique<ConnectTask>(socketIdentifier, ipAddress, port, std::move(connectionCallback));
}

}
#endif
