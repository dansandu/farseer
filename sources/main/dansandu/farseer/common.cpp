#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/expected.hpp"
#include "dansandu/journey/exception.hpp"
#include "dansandu/journey/logging.hpp"

#include <algorithm>

using dansandu::farseer::expected::responseAbortedErrorCode;
using dansandu::farseer::expected::responseAbortedErrorMessage;
using dansandu::farseer::expected::responseTimeoutErrorCode;
using dansandu::farseer::expected::responseTimeoutErrorMessage;
using dansandu::journey::exception::WideException;

namespace dansandu::farseer
{

const char* toString(const SocketEvent event)
{
    switch (event)
    {
    case SocketEvent::serverOpen:
        return "serverOpen";
    case SocketEvent::serverClosed:
        return "serverClosed";
    case SocketEvent::serverAborted:
        return "serverAborted";
    case SocketEvent::clientOpen:
        return "clientOpen";
    case SocketEvent::clientBytesReceived:
        return "clientBytesReceived";
    case SocketEvent::clientBytesSent:
        return "clientBytesSent";
    case SocketEvent::clientClosed:
        return "clientClosed";
    case SocketEvent::clientAborted:
        return "clientAborted";
    default:
        THROW(std::logic_error, "Unknown SocketEvent");
    }
}

OneShotResponseConsumer::OneShotResponseConsumer(
    const ProtocolIdentifier responseIdentifier, const FailedResponseProducer failedResponseProducer,
    const std::optional<std::chrono::steady_clock::time_point> deadline,
    UniqueFunction<void(std::any&&)>&& responseConsumer
)
    : responseIdentifier_{responseIdentifier},
      failedResponseProducer_{failedResponseProducer},
      deadline_{deadline},
      responseConsumer_{std::move(responseConsumer)}
{
}

OneShotResponseConsumer::OneShotResponseConsumer(OneShotResponseConsumer&& other) noexcept
    : responseIdentifier_{other.responseIdentifier_},
      failedResponseProducer_{other.failedResponseProducer_},
      deadline_{other.deadline_},
      responseConsumer_{std::move(other.responseConsumer_)}
{
    other.failedResponseProducer_ = nullptr;
}

OneShotResponseConsumer& OneShotResponseConsumer::operator=(OneShotResponseConsumer&& other) noexcept
{
    if (this != &other)
    {
        invokeFailedResponse();

        responseIdentifier_ = other.responseIdentifier_;
        failedResponseProducer_ = other.failedResponseProducer_;
        deadline_ = other.deadline_;
        responseConsumer_ = std::move(other.responseConsumer_);

        other.failedResponseProducer_ = nullptr;
    }

    return *this;
}

OneShotResponseConsumer::~OneShotResponseConsumer() noexcept
{
    invokeFailedResponse();
}

std::optional<std::chrono::steady_clock::time_point> OneShotResponseConsumer::getDeadline() const noexcept
{
    return deadline_;
}

bool OneShotResponseConsumer::hasTimedOut() const noexcept
{
    const auto now = std::chrono::steady_clock::now();

    return deadline_.has_value() && deadline_.value() <= now;
}

void OneShotResponseConsumer::invokeFailedResponse() noexcept
{
    if (failedResponseProducer_ == nullptr)
    {
        return;
    }

    const auto timedOut = hasTimedOut();

    auto failedResponse = timedOut ? failedResponseProducer_(responseTimeoutErrorCode, responseTimeoutErrorMessage)
                                   : failedResponseProducer_(responseAbortedErrorCode, responseAbortedErrorMessage);

    failedResponseProducer_ = nullptr;

    wrapInTryCatchAndInvokeProtocolConsumer(responseIdentifier_, responseConsumer_, std::move(failedResponse));
}

bool OneShotResponseConsumer::invoke(std::any&& response) noexcept
{
    if (failedResponseProducer_ == nullptr)
    {
        return false;
    }

    if (hasTimedOut())
    {
        auto failedResponse = failedResponseProducer_(responseTimeoutErrorCode, responseTimeoutErrorMessage);

        failedResponseProducer_ = nullptr;

        wrapInTryCatchAndInvokeProtocolConsumer(responseIdentifier_, responseConsumer_, std::move(failedResponse));

        return true;
    }

    failedResponseProducer_ = nullptr;

    wrapInTryCatchAndInvokeProtocolConsumer(responseIdentifier_, responseConsumer_, std::move(response));

    return true;
}

std::chrono::steady_clock::time_point getDeadlineFromTimeout(const std::chrono::milliseconds timeout) noexcept
{
    const auto now = std::chrono::steady_clock::now();

    const auto maximumTimeout =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::time_point::max() - now);

    return now + std::clamp(timeout, std::chrono::milliseconds::zero(), maximumTimeout);
}

void wrapInTryCatchAndInvokeProtocolConsumer(
    const ProtocolIdentifier protocolIdentifier, const UniqueFunction<void(std::any&&)>& protocolConsumer,
    std::any&& protocol
)
{
    try
    {
        protocolConsumer(std::move(protocol));
    }
    catch (const WideException& exception)
    {
        LOG_ERROR(
            "While invoking the consumer for the protocol with identifier ", protocolIdentifier,
            " a wide exception was caught with the message: ", exception.getMessage()
        );
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR(
            "While invoking the consumer for the protocol with identifier ", protocolIdentifier,
            " an exception was caught with the message: ", exception.what()
        );
    }
    catch (...)
    {
        LOG_ERROR(
            "While invoking the consumer for the protocol with identifier ", protocolIdentifier,
            " an unknown exception was caught"
        );
    }
}

}
