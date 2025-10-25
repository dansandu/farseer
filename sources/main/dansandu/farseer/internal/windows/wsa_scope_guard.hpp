#pragma once

#include <stdexcept>

#include <winsock2.h>

namespace dansandu::farseer::internal::windows::wsa_scope_guard
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

    WsaScopeGuard(const WsaScopeGuard&) = delete;

    WsaScopeGuard(WsaScopeGuard&& other) noexcept : initializeWsa_{other.initializeWsa_}
    {
        other.initializeWsa_ = false;
    }

    WsaScopeGuard& operator=(const WsaScopeGuard&) = delete;

    WsaScopeGuard& operator=(WsaScopeGuard&& other) noexcept
    {
        if (this != &other)
        {
            initializeWsa_ = other.initializeWsa_;
            other.initializeWsa_ = false;
        }
        return *this;
    }

    ~WsaScopeGuard() noexcept
    {
        if (initializeWsa_)
        {
            WSACleanup();
        }
    }

private:
    bool initializeWsa_;
};

}
