#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dansandu::farseer::internal::linux::linux_socket
{

enum class SocketType
{
    unbound,
    listening,
    accepted,
    connection,
};

class LinuxSocket
{
public:
    LinuxSocket(const LinuxSocket&) = delete;
    LinuxSocket& operator=(const LinuxSocket&) = delete;

    static LinuxSocket listen(const std::string& ipAddress, const int port, const int eventPollFileDescriptor);

    static LinuxSocket connect(const std::string& ipAddress, const int port, const int eventPollFileDescriptor);

    LinuxSocket(LinuxSocket&& other) noexcept;

    LinuxSocket& operator=(LinuxSocket&& other) noexcept;

    ~LinuxSocket() noexcept;

    std::optional<LinuxSocket> accept();

    void send(const uint8_t* const bytes, const size_t numberOfBytes);

    std::vector<uint8_t> receive();

    SocketType getSocketType() const
    {
        return socketType_;
    }

    const std::string& getIpAddress() const
    {
        return ipAddress_;
    }

    int getPort() const
    {
        return port_;
    }

    int getFileDescriptor() const
    {
        return socket_;
    }

private:
    LinuxSocket(SocketType socketType, const int socket, const int eventPollFileDescriptor,
                const std::string& ipAddress, const int port);

    SocketType socketType_;
    int socket_;
    int eventPollFileDescriptor_;
    std::string ipAddress_;
    int port_;
};

}
