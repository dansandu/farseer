#if defined(__linux__)
#include "dansandu/farseer/internal/linux/linux_socket_provider_implementation.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/linux/connect_task.hpp"
#include "dansandu/farseer/internal/linux/error.hpp"
#include "dansandu/farseer/internal/linux/event_poll.hpp"
#include "dansandu/farseer/internal/linux/listen_task.hpp"
#include "dansandu/farseer/internal/linux/send_bytes_task.hpp"
#include "dansandu/farseer/internal/linux/socket_container.hpp"
#include "dansandu/farseer/internal/linux/task.hpp"
#include "dansandu/farseer/internal/linux/task_queue.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/socket_provider_implementation.hpp"
#include "dansandu/journey/logging.hpp"

#include <sys/epoll.h>

#include <cstring>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>

using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::linux::connect_task::createConnectTask;
using dansandu::farseer::internal::linux::error::getLastErrorMessage;
using dansandu::farseer::internal::linux::event_poll::EventPoll;
using dansandu::farseer::internal::linux::listen_task::createListenTask;
using dansandu::farseer::internal::linux::send_bytes_task::createSendBytesTask;
using dansandu::farseer::internal::linux::socket_container::SocketContainer;
using dansandu::farseer::internal::linux::task::ITask;
using dansandu::farseer::internal::linux::task_queue::TaskQueue;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::socket_provider_implementation::ISocketProviderImplementation;
using dansandu::journey::exception::WideException;

namespace dansandu::farseer::internal::linux::linux_socket_provider_implementation
{

namespace
{

class LinuxSocketProviderImplementation : public ISocketProviderImplementation
{
public:
    LinuxSocketProviderImplementation()
        : socketIdentifierSequencer_{invalidSocketIdentifier.getUnderlying() + 1u},
          protocolSequencer_{},
          eventPoll_{},
          taskQueue_{eventPoll_.getEventPollFileDescriptor()},
          socketContainer_{eventPoll_.getEventPollFileDescriptor()},
          thread_{&LinuxSocketProviderImplementation::consumeEvents, this}
    {
    }

    ~LinuxSocketProviderImplementation() noexcept
    {
        taskQueue_.insert(nullptr);

        thread_.join();
    }

    SocketIdentifier listen(const std::string& ipAddress, const int port,
                            ConnectionCallback&& connectionCallback) override
    {
        const auto socketIdentifier = socketIdentifierSequencer_.generate();
        taskQueue_.insert(createListenTask(socketIdentifier, ipAddress, port, std::move(connectionCallback)));
        return socketIdentifier;
    }

    SocketIdentifier connect(const std::string& ipAddress, const int port,
                             ConnectionCallback&& connectionCallback) override
    {
        const auto socketIdentifier = socketIdentifierSequencer_.generate();
        taskQueue_.insert(createConnectTask(socketIdentifier, ipAddress, port, std::move(connectionCallback)));
        return socketIdentifier;
    }

    void sendBytes(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) override
    {
        taskQueue_.insert(createSendBytesTask(socketIdentifier, std::move(bytes)));
    }

    void sendRequest(const SocketIdentifier, const ProtocolSequenceNumber, std::vector<uint8_t>&&,
                     UniqueFunction<void(std::any&&)>&&) override
    {
        // operations_.createSendRequestOperation(socketIdentifier, sequenceNumber, std::move(bytes),
        //                                        std::move(expectedResponseConsumer));
    }

    void registerMessageConsumer(const SocketIdentifier, const ProtocolIdentifier,
                                 UniqueFunction<void(std::any&&)>&&) override
    {
        // operations_.createRegisterMessageConsumerOperation(socketIdentifier, protocolIdentifier,
        //                                                    std::move(messageConsumer));
    }

    void registerRequestCallback(const SocketIdentifier, const ProtocolIdentifier,
                                 UniqueFunction<std::any(std::any&&)>&&) override
    {
        // operations_.createRegisterRequestCallbackOperation(socketIdentifier, protocolIdentifier,
        //                                                    std::move(requestCallback));
    }

    void close(const SocketIdentifier) override
    {
        // operations_.createCloseOperation(socketIdentifier);
    }

    ProtocolSequenceNumber generateSequenceNumber() override
    {
        return protocolSequencer_.generate();
    }

private:
    bool consumeTasks(std::vector<std::unique_ptr<ITask>>& tasksBuffer)
    {
        taskQueue_.transfer(tasksBuffer);

        SCOPE_EXIT([&] { tasksBuffer.clear(); });

        for (auto& task : tasksBuffer)
        {
            if (task == nullptr)
            {
                return true;
            }

            const auto socketIdentifier = task->getSocketIdentifier().getUnderlying();
            const auto name = task->getName();

            LOG_DEBUG("Executing ", name, " with socket ID ", socketIdentifier);

            try
            {
                task->execute(socketContainer_);
            }
            catch (const WideException& exception)
            {
                LOG_ERROR("Error executing ", name, " with socket ID ", socketIdentifier, ": ", exception.getMessage());
            }
            catch (const std::exception& exception)
            {
                LOG_ERROR("Error executing ", name, " with socket ID ", socketIdentifier, ": ", exception.what());
            }
        }

        return false;
    }

    void consumeEventsWork()
    {
        const auto eventPollFileDescriptor = eventPoll_.getEventPollFileDescriptor();

        const auto eventFileDescriptor = taskQueue_.getEventFileDescriptor();

        auto events = std::vector<epoll_event>{};

        // preserve capacity to avoid reallocation
        auto tasksBuffer = std::vector<std::unique_ptr<ITask>>{};

        while (true)
        {
            const auto maximumNumberOfEvents = 1024;

            const auto numberOfSockets = static_cast<int>(socketContainer_.getNumberOfSockets());

            // add one event for the task queue event file descriptor
            const auto numberOfEvents = std::min(numberOfSockets + 1, maximumNumberOfEvents);

            events.resize(numberOfEvents);

            const auto timeout = -1;
            const auto numberOfPendingEvents =
                ::epoll_wait(eventPollFileDescriptor, events.data(), static_cast<int>(events.size()), timeout);

            if (numberOfPendingEvents == -1)
            {
                WTHROW(InternalSocketError, "Error waiting for events: ", getLastErrorMessage());
            }

            const auto abort = consumeTasks(tasksBuffer);

            if (abort)
            {
                LOG_DEBUG("Received abort task");
                return;
            }

            for (auto index = 0; index < numberOfPendingEvents; ++index)
            {
                if (events[index].data.fd == eventFileDescriptor)
                {
                    uint64_t counter = 0;

                    const auto numberOfBytesRead = ::read(eventFileDescriptor, &counter, sizeof(counter));

                    if (numberOfBytesRead == -1)
                    {
                        WTHROW(InternalSocketError, "Error reading event: ", getLastErrorMessage());
                    }
                }
                else
                {
                    socketContainer_.handleSocketEvent(events[index].data.fd, events[index].events,
                                                       socketIdentifierSequencer_);
                }
            }

            events.clear();
        }
    }

    void consumeEvents()
    {
        LOG_DEBUG("Started events consumer thread");

        try
        {
            consumeEventsWork();

            LOG_DEBUG("Gracefully exited events consumer thread");
        }
        catch (const WideException& exception)
        {
            LOG_CRITICAL("Events consumer thread exited with wide exception: ", exception.getMessage());
        }
        catch (const std::exception& exception)
        {
            LOG_CRITICAL("Events consumer thread exited with exception: ", exception.what());
        }
    }

    Sequencer<SocketIdentifier> socketIdentifierSequencer_;
    Sequencer<ProtocolSequenceNumber> protocolSequencer_;
    EventPoll eventPoll_;
    TaskQueue taskQueue_;
    SocketContainer socketContainer_;
    std::thread thread_;
};

}

std::shared_ptr<ISocketProviderImplementation> createLinuxSocketProviderImplementation()
{
    return std::make_shared<LinuxSocketProviderImplementation>();
}

}
#endif
