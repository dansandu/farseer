#pragma once

#include <cstdint>
#include <string>

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

    void send(const uint8_t* const bytes, const size_t numberOfBytes);

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
    LinuxSocket();

    SocketType socketType_;
    int socket_;
    std::string ipAddress_;
    int port_;
};

}
