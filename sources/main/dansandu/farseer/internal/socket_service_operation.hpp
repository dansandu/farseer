#pragma once

#include "dansandu/ballotin/logging.hpp"
#include "dansandu/farseer/common.hpp"

#include <winsock2.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace dansandu::farseer::internal::socket_service_operation
{

static constexpr auto socketServiceOperationBufferSize = 4096;

enum class SocketServiceOperationType
{
    listen,
    pendingAccept,
    connect,
    pendingConnect,
    pendingReceiveMessage,
    sendMessage,
    pendingSendMessage,
    close,
};

const char* toString(const SocketServiceOperationType operationType);

struct SocketServiceOperation : public WSAOVERLAPPED
{
    SocketServiceOperationType operationType;
    SocketServiceId serviceId;
    std::wstring ipAddress;
    int port;
    BytesType message;
    CallbackType callback;
    DWORD numberOfBytes;
    char buffer[socketServiceOperationBufferSize];
};

class SocketServiceOperationContainer
{
public:
    SocketServiceOperation* push(SocketServiceOperation operation);

    std::unique_ptr<SocketServiceOperation> pop(LPOVERLAPPED overlapped);

    SocketServiceOperation* get(LPOVERLAPPED overlapped) const;

private:
    std::map<LPOVERLAPPED, std::unique_ptr<SocketServiceOperation>> operations_;
    mutable std::mutex operationsLock_;
};

}
