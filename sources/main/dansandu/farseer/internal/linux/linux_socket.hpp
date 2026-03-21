#pragma once

namespace dansandu::farseer::internal::linux::linux_socket
{

class LinuxSocket
{
public:
    LinuxSocket();

    LinuxSocket(const LinuxSocket&) = delete;

    LinuxSocket(LinuxSocket&& other) noexcept;

    LinuxSocket& operator=(const LinuxSocket&) = delete;

    LinuxSocket& operator=(LinuxSocket&& other) noexcept;

    ~LinuxSocket() noexcept;

    void listen(const std::string& ipAddress, const int port, const int eventPollFileDescriptor);

    const std::string& getIpAddress() const
    {
        return ipAddress_;
    }

    int getPort() const
    {
        return port_;
    }

private:
    int socket_;
    std::string ipAddress_;
    int port_;
};

}
