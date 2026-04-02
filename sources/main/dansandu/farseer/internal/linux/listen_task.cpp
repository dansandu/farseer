#if defined(__linux__)
#include "dansandu/farseer/internal/linux/listen_task.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/i_task_scheduler.hpp"
#include "dansandu/farseer/internal/linux/linux_socket.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/journey/logging.hpp"

using dansandu::farseer::internal::linux::i_task_scheduler::ITask;
using dansandu::farseer::internal::linux::i_task_scheduler::ITaskScheduler;
using dansandu::farseer::internal::linux::i_task_scheduler::Socket;
using dansandu::farseer::internal::linux::linux_socket::LinuxSocket;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;

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

    void execute(ITaskScheduler& taskScheduler) override
    {
        auto& socket = taskScheduler.insertSocket(
            socketIdentifier_,
            Socket{
                .socketIdentifier = socketIdentifier_,
                .socket = LinuxSocket::listen(ipAddress_, port_, taskScheduler.getEventPollFileDescriptor()),
                .protocolReader =
                    ProtocolReader{
                        [&](const SocketIdentifier receivingSocketIdentifier, std::vector<uint8_t>&& response)
                        { taskScheduler.scheduleSendBytesTask(receivingSocketIdentifier, std::move(response)); }},
                .listeningSocketIdentifier = invalidSocketIdentifier,
                .connectionCallback = std::move(connectionCallback_),
            });

        socket.connectionCallback(SocketEvent::serverOpen, socketIdentifier_);

        LOG_INFO("Opened listening socket with ID ", socketIdentifier_.getUnderlying(), " and address ",
                 socket.socket.getIpAddress(), ':', socket.socket.getPort());
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
