#if defined(__linux__)
#include "dansandu/farseer/internal/linux/send_bytes_task.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/i_task_scheduler.hpp"
#include "dansandu/farseer/internal/linux/linux_socket.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"

using dansandu::farseer::internal::linux::i_task_scheduler::ITask;
using dansandu::farseer::internal::linux::i_task_scheduler::ITaskScheduler;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;

namespace dansandu::farseer::internal::linux::send_bytes_task
{

class SendBytesTask : public ITask
{
public:
    SendBytesTask(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes)
        : socketIdentifier_{socketIdentifier}, bytes_{std::move(bytes)}
    {
    }

    const char* getName() const override
    {
        return "SendBytesTask";
    }

    SocketIdentifier getSocketIdentifier() const override
    {
        return socketIdentifier_;
    }

    void execute(ITaskScheduler& taskScheduler) override
    {
        auto& socket = taskScheduler.getSocketOrThrow(socketIdentifier_);

        socket.socket.send(bytes_.data(), bytes_.size());
    }

private:
    const SocketIdentifier socketIdentifier_;
    std::vector<uint8_t> bytes_;
};

std::unique_ptr<ITask> createSendBytesTask(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes)
{
    return std::make_unique<SendBytesTask>(socketIdentifier, std::move(bytes));
}

}
#endif
