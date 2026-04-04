#if defined(__linux__)
#include "dansandu/farseer/internal/linux/send_bytes_task.hpp"
#include "dansandu/farseer/common.hpp"

using dansandu::farseer::internal::linux::socket_container::SocketContainer;
using dansandu::farseer::internal::linux::task::ITask;

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

    void execute(SocketContainer& socketContainer) override
    {
        socketContainer.sendBytes(socketIdentifier_, bytes_);
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
