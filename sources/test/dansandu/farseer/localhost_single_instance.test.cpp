#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/socket_provider.hpp"
#include "dansandu/farseer/stress_test_helper.hpp"
#include "dansandu/journey/logging.hpp"
#include "dansandu/radiance/radiance.hpp"

#include <future>
#include <thread>

using dansandu::farseer::socket_provider::SocketProvider;
using dansandu::farseer::stress_test_helper::getExpectedErrorCode;
using dansandu::farseer::stress_test_helper::getExpectedErrorMessage;
using dansandu::farseer::stress_test_helper::getExpectedReceived;
using dansandu::farseer::stress_test_helper::isFailing;
using dansandu::farseer::stress_test_helper::numberOfClients;
using dansandu::farseer::stress_test_helper::runClient;
using dansandu::farseer::stress_test_helper::serverTimeout;
using dansandu::farseer::stress_test_helper::startServer;
using dansandu::farseer::stress_test_helper::StressResult;
using dansandu::farseer::stress_test_helper::waitForFutureOrThrow;

TEST_CASE("localhost_single_instance")
{
    const auto initializeWsa = true;
    const auto socketProvider = SocketProvider{initializeWsa};

    LOG_INFO("Starting server...");

    const auto listenerId = startServer(socketProvider);

    SCOPE_EXIT([&]() { socketProvider.close(listenerId); });

    auto futures = std::vector<std::future<std::vector<StressResult>>>{};

    LOG_INFO("Spawning ", numberOfClients, " clients...");

    for (auto clientIndex = size_t{}; clientIndex < numberOfClients; ++clientIndex)
    {
        futures.push_back(
            std::async(
                std::launch::async, [socketProvider, clientIndex]() { return runClient(socketProvider, clientIndex); }
            )
        );
    }

    LOG_INFO("Waiting for clients to finish...");

    for (auto index = size_t{}; index < futures.size(); ++index)
    {
        const auto results = waitForFutureOrThrow(futures[index], "Client request", serverTimeout);

        for (const auto& [request, response] : results)
        {
            if (isFailing(request))
            {
                REQUIRE(response.failure());

                REQUIRE(getExpectedErrorCode(request) == response.getErrorCode());

                REQUIRE(getExpectedErrorMessage() == response.getErrorMessage());
            }
            else
            {
                REQUIRE(response.success());

                REQUIRE(getExpectedReceived(request) == response.getValue().received);
            }
        }

        LOG_INFO(index + 1U, "/", futures.size(), " clients finished");
    }

    LOG_INFO("All requests are done!");
}
