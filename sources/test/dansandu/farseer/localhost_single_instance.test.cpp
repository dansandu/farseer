#include "dansandu/ballotin/exception.hpp"
#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/sample_protocol.g.hpp"
#include "dansandu/farseer/socket_provider.hpp"
#include "dansandu/journey/logging.hpp"
#include "dansandu/radiance/radiance.hpp"

#include <future>
#include <thread>

using dansandu::farseer::Expected;
using dansandu::farseer::RequestProtocolError;
using dansandu::farseer::SocketEvent;
using dansandu::farseer::SocketIdentifier;
using dansandu::farseer::sample_protocol::StressRequest;
using dansandu::farseer::socket_provider::SocketProvider;

using StressResponse = dansandu::farseer::sample_protocol::StressRequest::Response;

namespace
{

constexpr auto localhost = "127.0.0.1";

constexpr auto serverPort = 34777;

constexpr auto clientTimeout = std::chrono::seconds(3);

constexpr auto serverTimeout = std::chrono::seconds(4);

constexpr auto responseErrorMessage = "error message";

template<typename T>
T waitForFutureOrThrow(std::future<T>& future, const char* const instance, const std::chrono::milliseconds timeout)
{
    const auto connectionStatus = future.wait_for(timeout);

    if (connectionStatus == std::future_status::timeout)
    {
        THROW(std::runtime_error, instance, " timed out");
    }

    if (connectionStatus == std::future_status::deferred)
    {
        THROW(std::runtime_error, instance, " was deferred");
    }

    if constexpr (std::is_same_v<T, void>)
    {
        future.get();
    }
    else
    {
        return future.get();
    }
}

std::pair<StressRequest, Expected<StressResponse>> createClient(const SocketProvider& socketProvider,
                                                                const StressRequest request)
{
    auto openPromise = std::promise<void>{};
    auto openFuture = openPromise.get_future();

    const auto connectionId = socketProvider.connect(
        localhost, serverPort,
        [openPromise = std::move(openPromise)](const SocketEvent event, const SocketIdentifier) mutable
        {
            if (event == SocketEvent::clientOpen)
            {
                openPromise.set_value();
            }
        });

    SCOPE_EXIT([&] { socketProvider.close(connectionId); });

    waitForFutureOrThrow(openFuture, "Client connection", clientTimeout);

    auto responsePromise = std::promise<Expected<StressResponse>>{};
    auto responseFuture = responsePromise.get_future();

    socketProvider.sendRequest(
        connectionId, request,
        [responsePromise = std::move(responsePromise)](Expected<StressResponse>&& response) mutable
        { responsePromise.set_value(std::move(response)); });

    return {request, waitForFutureOrThrow(responseFuture, "Client response", clientTimeout)};
}

uint32_t salted(uint32_t value)
{
    constexpr auto salt = 35123074U;
    return salt ^ value;
}

}

TEST_CASE("localhost_single_instance")
{
    const auto initializeWsa = true;
    const auto socketProvider = SocketProvider{initializeWsa};

    auto openPromise = std::promise<void>{};
    auto openFuture = openPromise.get_future();

    LOG_INFO("Opening listening socket...");

    const auto listenerId = socketProvider.listen(
        localhost, serverPort,
        [openPromise = std::move(openPromise)](const SocketEvent event, const SocketIdentifier) mutable
        {
            if (event == SocketEvent::serverOpen)
            {
                openPromise.set_value();
            }
        });

    SCOPE_EXIT([&]() { socketProvider.close(listenerId); });

    waitForFutureOrThrow(openFuture, "Server open", serverTimeout);

    LOG_INFO("Registering request callback...");

    socketProvider.registerRequestCallback<StressRequest>(listenerId,
                                                          [](StressRequest&& request)
                                                          {
                                                              if (request.sent % 2U == 0U)
                                                              {
                                                                  return StressResponse{
                                                                      .received = salted(request.sent),
                                                                  };
                                                              }
                                                              else
                                                              {
                                                                  throw RequestProtocolError{salted(request.sent),
                                                                                             responseErrorMessage};
                                                              }
                                                          });

    const auto requests = {
        4108274513U, 2679407135U, 3357528569U, 3675811652U, 3525994765U, 3902187997U, 3466800233U,
        3686493892U, 1995478447U, 3273354123U, 2047698237U, 1963749672U, 2910104339U, 3103644627U,
    };

    auto futures = std::vector<std::future<std::pair<StressRequest, Expected<StressResponse>>>>{};

    LOG_INFO("Spawning ", requests.size(), " clients...");

    for (const auto request : requests)
    {
        futures.push_back(std::async(std::launch::async, [socketProvider, request]()
                                     { return createClient(socketProvider, StressRequest{.sent = request}); }));
    }

    LOG_INFO("Waiting for clients to finish...");

    for (auto index = size_t{}; index < futures.size(); ++index)
    {
        const auto [request, response] = waitForFutureOrThrow(futures[index], "Client request", serverTimeout);

        if (request.sent % 2U == 0U)
        {
            REQUIRE(response.success());

            REQUIRE(request.sent == salted(response.getValue().received));
        }
        else
        {
            REQUIRE(response.failure());

            REQUIRE(request.sent == salted(response.getErrorCode()));

            REQUIRE(responseErrorMessage == response.getErrorMessage());
        }

        LOG_INFO(index + 1U, "/", futures.size(), " clients finished");
    }

    LOG_INFO("All requests are done!");
}
