#include "dansandu/farseer/stress_test_helper.hpp"
#include "dansandu/ballotin/hashing.hpp"
#include "dansandu/ballotin/scope.hpp"

using dansandu::ballotin::hashing::getHashCode32;
using dansandu::farseer::sample_protocol::StressRequest;
using dansandu::farseer::socket_provider::SocketProvider;

namespace dansandu::farseer::stress_test_helper
{

namespace
{

constexpr auto localhost = "127.0.0.1";

constexpr auto serverPort = 34777;

constexpr auto clientTimeout = std::chrono::seconds(30);

constexpr auto responseErrorMessage = "error message";

constexpr auto requestsPerClient = 4uz;

// Sizes close to the 16 KiB receive buffer place the end of a message right before, at or right after a read boundary,
// while the larger sizes span multiple reads.
constexpr size_t payloadSizes[] = {1uz, 2uz, 16383uz, 16384uz, 16385uz, 16386uz, 100000uz, 100001uz};

// Every request gets a distinct payload, so a response routed to the wrong request is detected.
StressRequest generateRequest(const size_t clientIndex, const size_t requestIndex)
{
    const auto size = payloadSizes[(clientIndex + requestIndex) % std::size(payloadSizes)];

    auto state = static_cast<uint32_t>(clientIndex * requestsPerClient + requestIndex + 1U);

    auto payload = std::string(size, '\0');

    for (auto& character : payload)
    {
        state = state * 1664525U + 1013904223U;
        character = static_cast<char>('a' + (state >> 24U) % 26U);
    }

    return StressRequest{.sent = std::move(payload)};
}

}

SocketIdentifier startServer(const SocketProvider& socketProvider)
{
    auto openPromise = std::promise<void>{};
    auto openFuture = openPromise.get_future();

    const auto listenerId = socketProvider.listen(
        localhost, serverPort,
        [openPromise = std::move(openPromise)](const SocketEvent event, const SocketIdentifier) mutable
        {
            if (event == SocketEvent::serverOpen)
            {
                openPromise.set_value();
            }
        }
    );

    SCOPE_FAILURE([&]() { socketProvider.close(listenerId); });

    waitForFutureOrThrow(openFuture, "Server open", serverTimeout);

    socketProvider.registerRequestCallback<StressRequest>(
        listenerId,
        [](StressRequest&& request)
        {
            if (isFailing(request))
            {
                throw RequestProtocolError{getExpectedErrorCode(request), getExpectedErrorMessage()};
            }

            return StressResponse{
                .received = getExpectedReceived(request),
            };
        }
    );

    return listenerId;
}

std::vector<StressResult> runClient(const SocketProvider& socketProvider, const size_t clientIndex)
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
        }
    );

    SCOPE_EXIT([&] { socketProvider.close(connectionId); });

    waitForFutureOrThrow(openFuture, "Client connection", clientTimeout);

    auto requests = std::vector<StressRequest>{};
    auto responseFutures = std::vector<std::future<Expected<StressResponse>>>{};

    for (auto requestIndex = size_t{}; requestIndex < requestsPerClient; ++requestIndex)
    {
        auto responsePromise = std::promise<Expected<StressResponse>>{};
        responseFutures.push_back(responsePromise.get_future());

        requests.push_back(generateRequest(clientIndex, requestIndex));

        socketProvider.sendRequest(
            connectionId, requests.back(),
            [responsePromise = std::move(responsePromise)](Expected<StressResponse>&& response) mutable
            { responsePromise.set_value(std::move(response)); }
        );
    }

    auto results = std::vector<StressResult>{};

    for (auto requestIndex = size_t{}; requestIndex < requestsPerClient; ++requestIndex)
    {
        results.emplace_back(
            std::move(requests[requestIndex]),
            waitForFutureOrThrow(responseFutures[requestIndex], "Client response", clientTimeout)
        );
    }

    return results;
}

bool isFailing(const StressRequest& request)
{
    return request.sent.size() % 2U == 1U;
}

uint32_t getExpectedErrorCode(const StressRequest& request)
{
    return getHashCode32(request.sent);
}

const char* getExpectedErrorMessage()
{
    return responseErrorMessage;
}

std::string getExpectedReceived(const StressRequest& request)
{
    return std::string(request.sent.crbegin(), request.sent.crend());
}

}
