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

constexpr auto timeout = std::chrono::seconds(6);

constexpr auto responseErrorMessage = "error message";

std::pair<StressRequest, Expected<StressResponse>> createClient(const StressRequest request)
{
    const auto initializeWsa = false;
    const auto client = SocketProvider{initializeWsa};

    auto openPromise = std::promise<void>{};
    auto openFuture = openPromise.get_future();

    const auto connectionId =
        client.connect(localhost, serverPort,
                       [openPromise = std::move(openPromise)](const SocketEvent event, const SocketIdentifier) mutable
                       {
                           if (event == SocketEvent::clientOpen)
                           {
                               openPromise.set_value();
                           }
                       });

    SCOPE_EXIT([&] { client.close(connectionId); });

    if (openFuture.wait_for(timeout) != std::future_status::ready)
    {
        return {request, Expected<StressResponse>::fromInternalServerError()};
    }

    auto responsePromise = std::promise<Expected<StressResponse>>{};
    auto responseFuture = responsePromise.get_future();

    client.sendRequest(connectionId, request,
                       [responsePromise = std::move(responsePromise)](Expected<StressResponse>&& response) mutable
                       { responsePromise.set_value(std::move(response)); });

    if (responseFuture.wait_for(timeout) == std::future_status::ready)
    {
        return {request, responseFuture.get()};
    }

    return {request, Expected<StressResponse>::fromInternalServerError()};
}

uint32_t salted(uint32_t value)
{
    constexpr auto salt = 35123074U;
    return salt ^ value;
}

}

TEST_CASE("localhost_multiple_instances")
{
    const auto initializeWsa = true;
    const auto server = SocketProvider{initializeWsa};

    auto openPromise = std::promise<void>{};
    auto openFuture = openPromise.get_future();

    LOG_INFO("Opening listening socket...");

    const auto listenerId =
        server.listen(localhost, serverPort,
                      [openPromise = std::move(openPromise)](const SocketEvent event, const SocketIdentifier) mutable
                      {
                          if (event == SocketEvent::serverOpen)
                          {
                              openPromise.set_value();
                          }
                      });

    REQUIRE(openFuture.wait_for(timeout) == std::future_status::ready);

    LOG_INFO("Registering request callback...");

    server.registerRequestCallback<StressRequest>(listenerId,
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
        futures.push_back(
            std::async(std::launch::async, [request]() { return createClient(StressRequest{.sent = request}); }));
    }

    LOG_INFO("Waiting for clients to finish...");

    for (auto index = size_t{}; index < futures.size(); ++index)
    {
        REQUIRE(futures[index].wait_for(timeout) == std::future_status::ready);

        LOG_INFO(index + 1U, "/", futures.size(), " clients finished");

        const auto& [request, expected] = futures[index].get();

        if (request.sent % 2U == 0U)
        {
            REQUIRE(expected.success());

            REQUIRE(request.sent == salted(expected.getValue().received));
        }
        else
        {
            REQUIRE(expected.failure());

            REQUIRE(request.sent == salted(expected.getErrorCode()));

            REQUIRE(responseErrorMessage == expected.getErrorMessage());
        }
    }

    LOG_INFO("Closing server...");

    server.close(listenerId);

    LOG_INFO("All requests are done!");
}
