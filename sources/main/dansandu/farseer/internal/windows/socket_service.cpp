#include "dansandu/farseer/internal/windows/socket_service.hpp"
#include "dansandu/farseer/exception.hpp"

using dansandu::farseer::exception::InternalSocketServiceException;

namespace dansandu::farseer::internal::windows::socket_service
{

SocketServiceContainerIterator getServiceOrThrow(SocketServiceContainer& services, const SocketServiceId serviceId)
{
    if (const auto servicePosition = services.find(serviceId); servicePosition != services.end())
    {
        return servicePosition;
    }

    WTHROW(InternalSocketServiceException, "Couldn't find service with ID ", serviceId.getInteger());
}

void closeSocketService(SocketServiceContainer& services, const SocketServiceId serviceId)
{
    if (const auto servicePosition = services.find(serviceId); servicePosition != services.end())
    {
        const auto& socket = servicePosition->second.socket;

        LOG_INFO("Socket with ID ", servicePosition->first.getInteger(), " and address ", socket.getIpAddress(), ':',
                 socket.getPort(), " was closed");

        if (servicePosition->second.listeningServiceId != InvalidServiceId)
        {
            const auto listeningServicePosition = services.find(servicePosition->second.listeningServiceId);

            if (listeningServicePosition != services.end())
            {
                listeningServicePosition->second.connectionCallback(
                    SocketServiceEvent::clientClosed, listeningServicePosition->first, servicePosition->first);
            }
        }
        else
        {
            servicePosition->second.connectionCallback(SocketServiceEvent::serverClosed, InvalidServiceId,
                                                       servicePosition->first);
        }

        services.erase(servicePosition->first);
    }
}

}
