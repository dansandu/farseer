#include "dansandu/ballotin/scope.hpp"
#include "dansandu/farseer/expected.hpp"
#include "dansandu/farseer/sample_protocol.g.hpp"
#include "dansandu/farseer/socket_provider.hpp"
#include "dansandu/journey/common.hpp"
#include "dansandu/journey/exception.hpp"
#include "dansandu/journey/logging.hpp"
#include "dansandu/radiance/radiance.hpp"

#include <chrono>
#include <future>
#include <optional>
#include <stdexcept>
#include <string>

using dansandu::farseer::Expected;
using dansandu::farseer::SocketEvent;
using dansandu::farseer::SocketIdentifier;
using dansandu::farseer::expected::responseAbortedErrorCode;
using dansandu::farseer::expected::responseTimeoutErrorCode;
using dansandu::farseer::sample_protocol::StressRequest;
using dansandu::farseer::socket_provider::SocketProvider;
using dansandu::journey::Level;
using dansandu::journey::logging::Logger;

namespace
{

using StressResponse = StressRequest::Response;

constexpr auto localhost = "127.0.0.1";
constexpr auto waitTimeout = std::chrono::seconds{10};

template<typename T>
void waitOrThrow(const std::future<T>& future)
{
    if (future.wait_for(waitTimeout) != std::future_status::ready)
    {
        THROW(std::runtime_error, "Timed out while waiting for the connection to be established");
    }
}

struct Connection
{
    SocketIdentifier listenerIdentifier;
    SocketIdentifier clientIdentifier;
    SocketIdentifier acceptedIdentifier;
};

Connection connect(const SocketProvider& socketProvider, const int port)
{
    auto serverOpenPromise = std::promise<void>{};
    auto serverOpenFuture = serverOpenPromise.get_future();

    auto acceptedPromise = std::promise<SocketIdentifier>{};
    auto acceptedFuture = acceptedPromise.get_future();

    const auto listenerIdentifier = socketProvider.listen(
        localhost, port,
        [serverOpenPromise = std::move(serverOpenPromise),
         acceptedPromise =
             std::move(acceptedPromise)](const SocketEvent event, const SocketIdentifier identifier) mutable
        {
            if (event == SocketEvent::serverOpen)
            {
                serverOpenPromise.set_value();
            }
            else if (event == SocketEvent::clientOpen)
            {
                acceptedPromise.set_value(identifier);
            }
        }
    );

    waitOrThrow(serverOpenFuture);

    auto clientOpenPromise = std::promise<void>{};
    auto clientOpenFuture = clientOpenPromise.get_future();

    const auto clientIdentifier = socketProvider.connect(
        localhost, port,
        [clientOpenPromise = std::move(clientOpenPromise)](const SocketEvent event, const SocketIdentifier) mutable
        {
            if (event == SocketEvent::clientOpen)
            {
                clientOpenPromise.set_value();
            }
        }
    );

    waitOrThrow(clientOpenFuture);

    waitOrThrow(acceptedFuture);

    return Connection{
        .listenerIdentifier = listenerIdentifier,
        .clientIdentifier = clientIdentifier,
        .acceptedIdentifier = acceptedFuture.get(),
    };
}

std::future<Expected<StressResponse>> sendRequest(
    const SocketProvider& socketProvider, const SocketIdentifier socketIdentifier,
    const std::optional<std::chrono::milliseconds> timeout
)
{
    auto responsePromise = std::promise<Expected<StressResponse>>{};
    auto responseFuture = responsePromise.get_future();

    socketProvider.sendRequest(
        socketIdentifier, StressRequest{.sent = "ping"}, timeout,
        [responsePromise = std::move(responsePromise)](Expected<StressResponse>&& response) mutable
        { responsePromise.set_value(std::move(response)); }
    );

    return responseFuture;
}

}

TEST_CASE("localhost_request_lifecycle")
{
    // Requests without a registered callback are skipped with a warning, which would otherwise fail the test. The level
    // is restored after the socket provider's thread has stopped because the warning is logged on that thread.
    const auto& logger = Logger::getGlobalInstance();
    const auto previousLevel = logger.getLevel();

    logger.setLevel(Level::error);

    SCOPE_EXIT([&]() { logger.setLevel(previousLevel); });

    const auto initializeWsa = true;
    const auto socketProvider = SocketProvider{initializeWsa};

    SECTION("request without a response times out")
    {
        const auto connection = connect(socketProvider, 34781);

        SCOPE_EXIT(
            [&]()
            {
                socketProvider.close(connection.clientIdentifier);
                socketProvider.close(connection.listenerIdentifier);
            }
        );

        // The server has no request callback registered, so the request is skipped and no response is ever sent.
        auto response = sendRequest(socketProvider, connection.clientIdentifier, std::chrono::milliseconds{50});

        REQUIRE(response.wait_for(waitTimeout) == std::future_status::ready);

        const auto result = response.get();

        REQUIRE(result.failure());

        REQUIRE(result.getErrorCode() == responseTimeoutErrorCode);
    }

    SECTION("pending request is aborted when its socket is closed")
    {
        const auto connection = connect(socketProvider, 34782);

        SCOPE_EXIT([&]() { socketProvider.close(connection.listenerIdentifier); });

        auto response = sendRequest(socketProvider, connection.clientIdentifier, std::nullopt);

        socketProvider.close(connection.clientIdentifier);

        REQUIRE(response.wait_for(waitTimeout) == std::future_status::ready);

        const auto result = response.get();

        REQUIRE(result.failure());

        REQUIRE(result.getErrorCode() == responseAbortedErrorCode);
    }

    SECTION("server receives the response to a request sent over an accepted socket")
    {
        const auto connection = connect(socketProvider, 34783);

        SCOPE_EXIT(
            [&]()
            {
                socketProvider.close(connection.clientIdentifier);
                socketProvider.close(connection.listenerIdentifier);
            }
        );

        socketProvider.registerRequestCallback<StressRequest>(
            connection.clientIdentifier,
            [](StressRequest&& request) { return StressResponse{.received = request.sent + " pong"}; }
        );

        auto response = sendRequest(socketProvider, connection.acceptedIdentifier, std::chrono::seconds{5});

        REQUIRE(response.wait_for(waitTimeout) == std::future_status::ready);

        const auto result = response.get();

        REQUIRE(result.success());

        REQUIRE(result.getValue().received == std::string{"ping pong"});
    }
}
