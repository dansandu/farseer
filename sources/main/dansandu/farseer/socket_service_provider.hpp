#pragma once

#include "dansandu/farseer/common.hpp"

#include <memory>
#include <string>

namespace dansandu::farseer::socket_service_provider
{

class PRALINE_EXPORT SocketServiceProvider
{
public:
    explicit SocketServiceProvider(bool initializeWsa);

    ~SocketServiceProvider();

    void listen(std::wstring ipAddress, const int port, CallbackType callback) const;

    void connect(std::wstring ipAddress, const int port, CallbackType callback) const;

    void send(const SocketServiceId serviceId, BytesType message) const;

    void close(const SocketServiceId serviceId) const;

private:
    std::shared_ptr<void> implementation_;
};

}
