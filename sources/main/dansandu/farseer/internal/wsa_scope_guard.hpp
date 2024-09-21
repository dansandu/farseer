#pragma once

#include <winsock2.h>

#include <stdexcept>

namespace dansandu::farseer::internal::wsa_scope_guard
{

class WsaScopeGuard
{
public:
    explicit WsaScopeGuard(const bool initializeWsa) : initializeWsa_{initializeWsa}
    {
        if (initializeWsa_)
        {
            auto wsaData = WSADATA{};
            const auto wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (wsaResult != NO_ERROR)
            {
                THROW(std::runtime_error, "WSAStartup failed with error ", wsaResult);
            }
        }
    }

    ~WsaScopeGuard() noexcept
    {
        if (initializeWsa_)
        {
            WSACleanup();
        }
    }

private:
    const bool initializeWsa_;
};

}
