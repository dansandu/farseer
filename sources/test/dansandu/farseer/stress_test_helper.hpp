#pragma once

#include "dansandu/farseer/sample_protocol.g.hpp"
#include "dansandu/farseer/socket_provider.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace dansandu::farseer::stress_test_helper
{

using StressResponse = dansandu::farseer::sample_protocol::StressRequest::Response;

using StressResult = std::pair<dansandu::farseer::sample_protocol::StressRequest, Expected<StressResponse>>;

constexpr auto serverTimeout = std::chrono::seconds(30);

constexpr auto numberOfClients = 64uz;

// Opens the listening socket and registers the stress request callback. The caller is responsible for closing the
// returned listening socket.
SocketIdentifier startServer(const dansandu::farseer::socket_provider::SocketProvider& socketProvider);

// Connects to the server and sends all the client's requests before waiting for any response, so multiple requests are
// pending on the same connection.
std::vector<StressResult>
runClient(const dansandu::farseer::socket_provider::SocketProvider& socketProvider, const size_t clientIndex);

// The server answers requests with odd payload sizes with this error code and error message.
bool isFailing(const dansandu::farseer::sample_protocol::StressRequest& request);

uint32_t getExpectedErrorCode(const dansandu::farseer::sample_protocol::StressRequest& request);

const char* getExpectedErrorMessage();

// The server answers the other requests with the payload reversed.
std::string getExpectedReceived(const dansandu::farseer::sample_protocol::StressRequest& request);

}
