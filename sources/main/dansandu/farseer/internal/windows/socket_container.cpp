#if defined(_WIN32)
#include "dansandu/farseer/internal/windows/socket_container.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/journey/exception.hpp"
#include "dansandu/journey/logging.hpp"

using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::exception::SocketNotFoundError;
using dansandu::journey::exception::WideException;

namespace dansandu::farseer::internal::windows::socket_container
{

Socket& SocketContainer::insertSocket(const SocketIdentifier socketIdentifier, Socket&& socket)
{
    const auto [position, inserted] = sockets_.emplace(socketIdentifier, std::move(socket));

    if (!inserted)
    {
        WTHROW(
            InternalSocketError, "Couldn't insert socket with ID ", socketIdentifier,
            " because the ID is used by another socket"
        );
    }

    return position->second;
}

Socket& SocketContainer::getSocketOrThrow(const SocketIdentifier socketIdentifier)
{
    const auto position = sockets_.find(socketIdentifier);

    if (position != sockets_.end())
    {
        return position->second;
    }

    WTHROW(SocketNotFoundError, "Couldn't find socket with ID ", socketIdentifier);
}

void SocketContainer::eraseSocket(const SocketIdentifier socketIdentifier)
{
    const auto position = sockets_.find(socketIdentifier);

    if (position != sockets_.end())
    {
        SCOPE_EXIT([&] { sockets_.erase(position); });

        const auto& socket = position->second;

        try
        {
            if (socket.pendingAccept)
            {
                LOG_DEBUG("Pending accept socket with ID ", socketIdentifier, " was closed");

                return;
            }

            if (socket.listeningSocketIdentifier != invalidSocketIdentifier)
            {
                const auto listeningSocketPosition = sockets_.find(socket.listeningSocketIdentifier);

                if (listeningSocketPosition != sockets_.end())
                {
                    listeningSocketPosition->second.connectionCallback(SocketEvent::clientClosed, socketIdentifier);
                }
            }
            else
            {
                socket.connectionCallback(SocketEvent::serverClosed, socketIdentifier);
            }

            LOG_INFO(
                "Socket with ID ", socketIdentifier, " and address ", socket.socket.getIpAddress(), ":",
                socket.socket.getPort(), " was closed"
            );
        }
        catch (const WideException& wideException)
        {
            LOG_ERROR(
                "Wide exception was thrown while trying to close socket with ID ", socketIdentifier,
                " with message: ", wideException.getMessage()
            );
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR(
                "Exception was thrown while trying to close socket with ID ", socketIdentifier,
                " with message: ", exception.what()
            );
        }
        catch (...)
        {
            LOG_ERROR("Unknown exception was thrown while trying to close socket with ID ", socketIdentifier);
        }
    }
}

void SocketContainer::eraseAllSockets()
{
    sockets_.clear();
}

}
#endif
