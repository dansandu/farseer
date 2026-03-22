#pragma once

#include "dansandu/ballotin/exception.hpp"
#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/windows/error.hpp"
#include "dansandu/journey/logging.hpp"

#include <string>

// clang-format off
// Include in this order otherwise it doesn't compile.
// STD headers before windows headers otherwise it doesn't compile.
#include <winsock2.h>
#include <mswsock.h>
#include <ioapiset.h>
#include <ws2tcpip.h>
// clang-format on

namespace dansandu::farseer::internal::windows::windows_socket
{

class WindowsSocket
{
public:
    WindowsSocket(const HANDLE completionPort, const SocketIdentifier socketIdentifier);

    WindowsSocket(const WindowsSocket&) = delete;

    WindowsSocket(WindowsSocket&& other) noexcept;

    WindowsSocket& operator=(const WindowsSocket&) = delete;

    WindowsSocket& operator=(WindowsSocket&& other) noexcept;

    ~WindowsSocket() noexcept;

    void listen(const std::string& ipAddress, const int port);

    WindowsSocket postAccept(CHAR* const receiveBuffer, const DWORD receiveBufferSize,
                             const SocketIdentifier pendingAcceptSocketIdentifier, const HANDLE completionPort,
                             const LPWSAOVERLAPPED overlapped) const;

    void accept(const WindowsSocket& listeningSocket);

    void postConnect(const std::string& ipAddress, const int port, const LPWSAOVERLAPPED overlapped);

    void connect();

    void postReceive(CHAR* const receiveBuffer, const ULONG receiveBufferSize, const LPWSAOVERLAPPED overlapped) const;

    void postSend(CHAR* const bytesToSend, const ULONG numberOfBytesToSend, const LPWSAOVERLAPPED overlapped) const;

    const std::string& getIpAddress() const
    {
        return ipAddress_;
    }

    int getPort() const
    {
        return port_;
    }

private:
    SOCKET socket_;
    LPFN_ACCEPTEX acceptFunction_;
    LPFN_CONNECTEX connectFunction_;
    std::string ipAddress_;
    int port_;
};

}
