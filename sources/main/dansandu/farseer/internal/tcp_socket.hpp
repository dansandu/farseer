#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/socket_service_operation.hpp"

#include <mswsock.h>
#include <winsock2.h>

#include <string>

namespace dansandu::farseer::internal::tcp_socket
{

class TcpSocket
{
public:
    TcpSocket(const HANDLE completionPort, const SocketServiceId serviceId);

    TcpSocket(const TcpSocket&) = delete;

    TcpSocket(TcpSocket&& other) noexcept;

    TcpSocket& operator=(const TcpSocket&) = delete;

    TcpSocket& operator=(TcpSocket&& other) noexcept;

    ~TcpSocket() noexcept;

    void toggleBlocking(const bool block) const;

    void listen(std::wstring ipAddress, const int port);

    TcpSocket
    postAccept(const HANDLE completionPort,
               dansandu::farseer::internal::socket_service_operation::SocketServiceOperation* operation) const;

    void accept(const TcpSocket& listeningSocket);

    void postConnect(std::wstring ipAddress, const int port,
                     dansandu::farseer::internal::socket_service_operation::SocketServiceOperation* operation);

    void connect();

    void postReceive(dansandu::farseer::internal::socket_service_operation::SocketServiceOperation* operation) const;

    void postSend(dansandu::farseer::internal::socket_service_operation::SocketServiceOperation* operation) const;

    const std::wstring& getIpAddress() const
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
    std::wstring ipAddress_;
    int port_;
};

}
