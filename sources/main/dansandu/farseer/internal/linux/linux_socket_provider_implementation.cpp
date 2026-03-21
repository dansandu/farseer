#if defined(__linux__)
#include "dansandu/farseer/internal/linux/linux_socket_provider_implementation.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/linux/linux_socket.hpp"
#include "dansandu/farseer/internal/linux/task_scheduler.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/socket_provider_implementation.hpp"
#include "dansandu/journey/logging.hpp"

#include <map>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>

using dansandu::farseer::internal::linux::linux_socket::LinuxSocket;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::sequencer::Sequencer;
using dansandu::farseer::internal::socket_provider_implementation::ISocketProviderImplementation;

namespace dansandu::farseer::internal::linux::linux_socket_provider_implementation
{

namespace
{

class LinuxSocketProviderImplementation : public ISocketProviderImplementation
{
public:
    SocketIdentifier listen(const std::string& ipAddress, const int port,
                            ConnectionCallback&& connectionCallback) override
    {
        const auto socketIdentifier = socketIdentifierSequencer_.generate();

        const auto lock = std::lock_guard<std::mutex>{tasksMutex_};

        tasks_.push(
            [socketIdentifier, ipAddress, port, connectionCallback = std::move(connectionCallback)]()
            {
                auto tempSocket = LinuxSocket{};

                tempSocket.listen(ipAddress, port);

                sockets_.emplace(
                    socketIdentifier,
                    Socket{
                        .socket = std::move(tempSocket),
                        .protocolReader = ProtocolReader{[&](const SocketIdentifier receivingSocketIdentifier,
                                                             std::vector<uint8_t>&& response)
                                                         {
                                                             operationScheduler.scheduleSendBytesOperation(
                                                                 receivingSocketIdentifier, std::move(response));
                                                         }},
                        .listeningSocketIdentifier = invalidSocketIdentifier,
                        .connectionCallback = std::move(connectionCallback),
                    });
            });

        return socketIdentifier;
    }

    SocketIdentifier connect(const std::string& ipAddress, const int port,
                             ConnectionCallback&& connectionCallback) override
    {
        return operations_.createConnectOperation(ipAddress, port, std::move(connectionCallback));
    }

    ProtocolSequenceNumber generateSequenceNumber() override
    {
        return protocolSequencer_.generate();
    }

    void sendBytes(const SocketIdentifier socketIdentifier, std::vector<uint8_t>&& bytes) override
    {
        taskScheduler_.scheduleSendBytesTask(socketIdentifier, std::move(bytes));
    }

    void sendRequest(const SocketIdentifier socketIdentifier, const ProtocolSequenceNumber sequenceNumber,
                     std::vector<uint8_t>&& bytes, UniqueFunction<void(std::any&&)>&& expectedResponseConsumer) override
    {
        operations_.createSendRequestOperation(socketIdentifier, sequenceNumber, std::move(bytes),
                                               std::move(expectedResponseConsumer));
    }

    void registerMessageConsumer(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                 UniqueFunction<void(std::any&&)>&& messageConsumer) override
    {
        operations_.createRegisterMessageConsumerOperation(socketIdentifier, protocolIdentifier,
                                                           std::move(messageConsumer));
    }

    void registerRequestCallback(const SocketIdentifier socketIdentifier, const ProtocolIdentifier protocolIdentifier,
                                 UniqueFunction<std::any(std::any&&)>&& requestCallback) override
    {
        operations_.createRegisterRequestCallbackOperation(socketIdentifier, protocolIdentifier,
                                                           std::move(requestCallback));
    }

    void close(const SocketIdentifier socketIdentifier) override
    {
        operations_.createCloseOperation(socketIdentifier);
    }

private:
    Sequencer<ProtocolSequenceNumber> protocolSequencer_;
    TaskScheduler taskScheduler_;
};

}

std::shared_ptr<ISocketProviderImplementation> createLinuxSocketProviderImplementation()
{
    return std::make_shared<LinuxSocketProviderImplementation>();
}

}
#endif
