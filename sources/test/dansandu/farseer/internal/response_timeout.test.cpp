#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/expected.hpp"
#include "dansandu/farseer/internal/protocol_reader.hpp"
#include "dansandu/farseer/internal/response_consumer_container.hpp"
#include "dansandu/farseer/sample_protocol.g.hpp"
#include "dansandu/journey/common.hpp"
#include "dansandu/radiance/radiance.hpp"

#include <any>
#include <chrono>
#include <optional>
#include <utility>
#include <vector>

using dansandu::farseer::Expected;
using dansandu::farseer::getDeadlineFromTimeout;
using dansandu::farseer::OneShotResponseConsumer;
using dansandu::farseer::ProtocolSequenceNumber;
using dansandu::farseer::expected::responseAbortedErrorCode;
using dansandu::farseer::expected::responseTimeoutErrorCode;
using dansandu::farseer::internal::protocol_reader::ProtocolReader;
using dansandu::farseer::internal::response_consumer_container::ResponseConsumerContainer;
using dansandu::farseer::sample_protocol::MyRequest;
using dansandu::journey::Level;
using dansandu::radiance::Log;

namespace
{

using Clock = std::chrono::steady_clock;
using Response = Expected<MyRequest::Response>;

OneShotResponseConsumer makeConsumer(std::vector<Response>& responses, const std::optional<Clock::time_point> deadline)
{
    return OneShotResponseConsumer{
        MyRequest::Response::Metadata::getProtocolIdentifier(), MyRequest::Response::Metadata::getFailedResponse,
        deadline, [&responses](std::any&& response) { responses.push_back(std::any_cast<Response>(response)); }
    };
}

std::vector<uint8_t> makeResponseBytes(const ProtocolSequenceNumber sequenceNumber)
{
    return MyRequest::Response::Metadata::serializeWithHeader(
        Response::fromSuccess(MyRequest::Response{.contacts = {"Ava"}, .authenticationToken = 42}), sequenceNumber
    );
}

}

TEST_CASE("response_timeout")
{
    const auto past = Clock::now() - std::chrono::seconds{1};
    const auto future = Clock::now() + std::chrono::hours{1};
    const auto noDeadline = std::optional<Clock::time_point>{};

    const auto firstSequenceNumber = ProtocolSequenceNumber{1};
    const auto secondSequenceNumber = ProtocolSequenceNumber{2};

    // Declared before the containers because pending consumers are invoked when the containers are destroyed.
    auto responses = std::vector<Response>{};
    auto otherResponses = std::vector<Response>{};

    auto inboundBytes = std::vector<uint8_t>{};
    auto outboundBytes = std::vector<uint8_t>{};

    auto responseConsumerContainer = ResponseConsumerContainer{};

    auto protocolReader = ProtocolReader{};

    SECTION("no pending responses")
    {
        REQUIRE(!responseConsumerContainer.eraseTimedOutResponses().has_value());
    }

    SECTION("responses without a deadline never time out")
    {
        responseConsumerContainer.insert(firstSequenceNumber, makeConsumer(responses, noDeadline));

        REQUIRE(!responseConsumerContainer.eraseTimedOutResponses().has_value());

        REQUIRE(responses.empty());
    }

    SECTION("single expired response is erased and receives a timeout failure")
    {
        responseConsumerContainer.insert(firstSequenceNumber, makeConsumer(responses, past));

        REQUIRE(!responseConsumerContainer.eraseTimedOutResponses().has_value());

        REQUIRE(responses.size() == 1uz);

        REQUIRE(responses.at(0).failure());

        REQUIRE(responses.at(0).getErrorCode() == responseTimeoutErrorCode);

        REQUIRE(!responseConsumerContainer.eraseTimedOutResponses().has_value());

        REQUIRE(responses.size() == 1uz);
    }

    SECTION("late response is skipped after the consumer timed out")
    {
        responseConsumerContainer.insert(firstSequenceNumber, makeConsumer(responses, past));

        responseConsumerContainer.eraseTimedOutResponses();

        inboundBytes = makeResponseBytes(firstSequenceNumber);

        const auto skippedResponseLogs = std::vector<Log>{
            Log{Level::debug, L"Successfully read the response protocol 1498178261"},
            Log{Level::warning, L"The response protocol with identifier 1498178261 and sequence number 1 will be "
                                L"skipped because no consumer was found possibly due to a timeout"},
        };

        REQUIRE_LOG(
            skippedResponseLogs, protocolReader.readInboundBytes(inboundBytes, outboundBytes, responseConsumerContainer)
        );

        REQUIRE(inboundBytes.empty());

        REQUIRE(responses.size() == 1uz);

        REQUIRE(responses.at(0).getErrorCode() == responseTimeoutErrorCode);
    }

    SECTION("only expired responses are erased and the earliest remaining deadline is returned")
    {
        const auto later = future + std::chrono::hours{1};

        responseConsumerContainer.insert(firstSequenceNumber, makeConsumer(responses, past));

        responseConsumerContainer.insert(secondSequenceNumber, makeConsumer(otherResponses, future));

        responseConsumerContainer.insert(ProtocolSequenceNumber{3}, makeConsumer(otherResponses, later));

        const auto earliestDeadline = responseConsumerContainer.eraseTimedOutResponses();

        REQUIRE(earliestDeadline.has_value());

        REQUIRE(earliestDeadline.value() == future);

        REQUIRE(responses.size() == 1uz);

        REQUIRE(otherResponses.empty());
    }

    SECTION("response arriving past its deadline but before erasure receives a timeout failure")
    {
        responseConsumerContainer.insert(firstSequenceNumber, makeConsumer(responses, past));

        inboundBytes = makeResponseBytes(firstSequenceNumber);

        protocolReader.readInboundBytes(inboundBytes, outboundBytes, responseConsumerContainer);

        REQUIRE(responses.size() == 1uz);

        REQUIRE(responses.at(0).failure());

        REQUIRE(responses.at(0).getErrorCode() == responseTimeoutErrorCode);

        REQUIRE(!responseConsumerContainer.eraseTimedOutResponses().has_value());

        REQUIRE(responses.size() == 1uz);
    }

    SECTION("response arriving before its deadline succeeds and clears the deadline")
    {
        responseConsumerContainer.insert(firstSequenceNumber, makeConsumer(responses, future));

        REQUIRE(responseConsumerContainer.eraseTimedOutResponses().value() == future);

        inboundBytes = makeResponseBytes(firstSequenceNumber);

        protocolReader.readInboundBytes(inboundBytes, outboundBytes, responseConsumerContainer);

        REQUIRE(responses.size() == 1uz);

        REQUIRE(responses.at(0).success());

        REQUIRE(!responseConsumerContainer.eraseTimedOutResponses().has_value());
    }

    SECTION("response only clears its own deadline when several responses share it")
    {
        responseConsumerContainer.insert(firstSequenceNumber, makeConsumer(responses, future));

        responseConsumerContainer.insert(secondSequenceNumber, makeConsumer(otherResponses, future));

        inboundBytes = makeResponseBytes(firstSequenceNumber);

        protocolReader.readInboundBytes(inboundBytes, outboundBytes, responseConsumerContainer);

        REQUIRE(responses.size() == 1uz);

        REQUIRE(otherResponses.empty());

        REQUIRE(responseConsumerContainer.eraseTimedOutResponses().value() == future);

        inboundBytes = makeResponseBytes(secondSequenceNumber);

        protocolReader.readInboundBytes(inboundBytes, outboundBytes, responseConsumerContainer);

        REQUIRE(otherResponses.size() == 1uz);

        REQUIRE(!responseConsumerContainer.eraseTimedOutResponses().has_value());
    }

    SECTION("erased response is aborted and its deadline is removed")
    {
        responseConsumerContainer.insert(firstSequenceNumber, makeConsumer(responses, future));

        responseConsumerContainer.insert(secondSequenceNumber, makeConsumer(otherResponses, future));

        responseConsumerContainer.erase(firstSequenceNumber);

        REQUIRE(responses.size() == 1uz);

        REQUIRE(responses.at(0).failure());

        REQUIRE(responses.at(0).getErrorCode() == responseAbortedErrorCode);

        REQUIRE(otherResponses.empty());

        REQUIRE(responseConsumerContainer.eraseTimedOutResponses().value() == future);

        responseConsumerContainer.erase(secondSequenceNumber);

        REQUIRE(otherResponses.size() == 1uz);

        REQUIRE(!responseConsumerContainer.eraseTimedOutResponses().has_value());
    }

    SECTION("erasing an unknown sequence number has no effect")
    {
        responseConsumerContainer.insert(firstSequenceNumber, makeConsumer(responses, future));

        responseConsumerContainer.erase(secondSequenceNumber);

        REQUIRE(responses.empty());

        REQUIRE(responseConsumerContainer.eraseTimedOutResponses().value() == future);
    }

    SECTION("responses are routed to the given container")
    {
        auto acceptedSocketResponseConsumerContainer = ResponseConsumerContainer{};

        acceptedSocketResponseConsumerContainer.insert(firstSequenceNumber, makeConsumer(responses, noDeadline));

        inboundBytes = makeResponseBytes(firstSequenceNumber);

        protocolReader.readInboundBytes(inboundBytes, outboundBytes, acceptedSocketResponseConsumerContainer);

        REQUIRE(inboundBytes.empty());

        REQUIRE(responses.size() == 1uz);

        REQUIRE(responses.at(0).success());
    }

    SECTION("pending responses are aborted when the container is destroyed")
    {
        {
            auto localResponseConsumerContainer = ResponseConsumerContainer{};

            localResponseConsumerContainer.insert(firstSequenceNumber, makeConsumer(responses, future));

            REQUIRE(responses.empty());
        }

        REQUIRE(responses.size() == 1uz);

        REQUIRE(responses.at(0).failure());

        REQUIRE(responses.at(0).getErrorCode() == responseAbortedErrorCode);
    }

    SECTION("one shot response consumer")
    {
        const auto successfulResponse = Response::fromSuccess(MyRequest::Response{});

        SECTION("invoke delivers the response once")
        {
            auto consumer = makeConsumer(responses, future);

            REQUIRE(consumer.invoke(std::any{successfulResponse}));

            REQUIRE(!consumer.invoke(std::any{successfulResponse}));

            REQUIRE(responses.size() == 1uz);

            REQUIRE(responses.at(0).success());
        }

        SECTION("invoke past the deadline delivers a timeout failure")
        {
            auto consumer = makeConsumer(responses, past);

            REQUIRE(consumer.hasTimedOut());

            REQUIRE(consumer.invoke(std::any{successfulResponse}));

            REQUIRE(responses.size() == 1uz);

            REQUIRE(responses.at(0).getErrorCode() == responseTimeoutErrorCode);
        }

        SECTION("destruction delivers an aborted failure")
        {
            {
                const auto consumer = makeConsumer(responses, noDeadline);

                REQUIRE(!consumer.hasTimedOut());
            }

            REQUIRE(responses.size() == 1uz);

            REQUIRE(responses.at(0).getErrorCode() == responseAbortedErrorCode);
        }

        SECTION("destruction past the deadline delivers a timeout failure")
        {
            {
                const auto consumer = makeConsumer(responses, past);
            }

            REQUIRE(responses.size() == 1uz);

            REQUIRE(responses.at(0).getErrorCode() == responseTimeoutErrorCode);
        }

        SECTION("move construction transfers the single invocation")
        {
            {
                auto source = makeConsumer(responses, noDeadline);

                auto destination = OneShotResponseConsumer{std::move(source)};

                REQUIRE(responses.empty());

                REQUIRE(destination.invoke(std::any{successfulResponse}));
            }

            REQUIRE(responses.size() == 1uz);

            REQUIRE(responses.at(0).success());
        }

        SECTION("move assignment aborts the replaced consumer")
        {
            {
                auto replaced = makeConsumer(otherResponses, noDeadline);

                auto source = makeConsumer(responses, noDeadline);

                replaced = std::move(source);

                REQUIRE(otherResponses.size() == 1uz);

                REQUIRE(otherResponses.at(0).getErrorCode() == responseAbortedErrorCode);

                REQUIRE(responses.empty());

                REQUIRE(replaced.invoke(std::any{successfulResponse}));
            }

            REQUIRE(otherResponses.size() == 1uz);

            REQUIRE(responses.size() == 1uz);

            REQUIRE(responses.at(0).success());
        }
    }

    SECTION("deadline from timeout")
    {
        using std::chrono::milliseconds;

        SECTION("regular timeout")
        {
            const auto before = Clock::now();

            const auto deadline = getDeadlineFromTimeout(milliseconds{1000});

            REQUIRE(deadline >= before + milliseconds{1000});

            REQUIRE(deadline <= Clock::now() + milliseconds{1000});
        }

        SECTION("maximum timeout doesn't overflow")
        {
            const auto now = Clock::now();

            const auto deadline = getDeadlineFromTimeout(milliseconds::max());

            REQUIRE(deadline > now + std::chrono::hours{24});
        }

        SECTION("negative timeouts expire immediately")
        {
            const auto negativeDeadline = getDeadlineFromTimeout(milliseconds{-1000});

            const auto minimumDeadline = getDeadlineFromTimeout(milliseconds::min());

            const auto now = Clock::now();

            REQUIRE(negativeDeadline <= now);

            REQUIRE(minimumDeadline <= now);
        }
    }
}
