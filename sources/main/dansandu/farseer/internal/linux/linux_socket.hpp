#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace dansandu::farseer::internal::linux::linux_socket
{

enum class SocketType
{
    unbound,
    listening,
    accepted,
    connecting,
    connected,
};

class LinuxSocket
{
public:
    LinuxSocket() = delete;
    LinuxSocket(const LinuxSocket&) = delete;
    LinuxSocket& operator=(const LinuxSocket&) = delete;

    static LinuxSocket listen(const std::string& ipAddress, const int port);

    static LinuxSocket connect(const std::string& ipAddress, const int port);

    LinuxSocket(LinuxSocket&& other) noexcept;

    LinuxSocket& operator=(LinuxSocket&& other) noexcept;

    ~LinuxSocket() noexcept;

    std::optional<LinuxSocket> accept();

    void connected();

    bool sendBytes(const std::span<const uint8_t> bytes);

    std::vector<uint8_t> receiveBytes();

    const std::string& getIpAddress() const
    {
        return ipAddress_;
    }

    int getPort() const
    {
        return port_;
    }

    int getSocketFileDescriptor() const
    {
        return socket_;
    }

    SocketType getSocketType() const
    {
        return socketType_;
    }

private:
    LinuxSocket(const std::string& ipAddress, const int port, const int socket, const SocketType socketType);

    std::vector<uint8_t> outgoingBuffer_;
    std::string ipAddress_;
    int port_;
    int socket_;
    SocketType socketType_;
};

}
