#if defined(__linux__)
#include "dansandu/farseer/internal/linux/event_poll.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/linux/error.hpp"
#include "dansandu/journey/logging.hpp"

#include <cstring>
#include <unistd.h>

using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::linux::error::getLastErrorMessage;

namespace dansandu::farseer::internal::linux::event_poll
{

namespace
{

int createEventPollFileDescriptor()
{
    const auto flags = EPOLL_CLOEXEC;

    const auto eventPollFileDescriptor = ::epoll_create1(flags);

    if (eventPollFileDescriptor == -1)
    {
        WTHROW(InternalSocketError, "Error creating event poll: ", getLastErrorMessage());
    }

    return eventPollFileDescriptor;
}

}

EventPoll::EventPoll() : eventPollFileDescriptor_{createEventPollFileDescriptor()}
{
}

EventPoll::~EventPoll() noexcept
{
    const auto closeResult = ::close(eventPollFileDescriptor_);

    if (closeResult == -1)
    {
        LOG_ERROR("Error closing event poll: ", getLastErrorMessage());
    }
}

void EventPoll::subscribe(const int fileDescriptor, const uint32_t events)
{
    ::epoll_event event;

    std::memset(&event, 0, sizeof(event));

    event.events = events;
    event.data.fd = fileDescriptor;

    const auto subscribeResult = ::epoll_ctl(eventPollFileDescriptor_, EPOLL_CTL_ADD, fileDescriptor, &event);

    if (subscribeResult == -1)
    {
        WTHROW(InternalSocketError, "Error subscribing file descriptor to event poll: ", getLastErrorMessage());
    }
}

void EventPoll::setEvents(const int fileDescriptor, const uint32_t events)
{
    ::epoll_event event;

    std::memset(&event, 0, sizeof(event));

    event.events = events;
    event.data.fd = fileDescriptor;

    const auto modifyResult = ::epoll_ctl(eventPollFileDescriptor_, EPOLL_CTL_MOD, fileDescriptor, &event);

    if (modifyResult == -1)
    {
        WTHROW(InternalSocketError, "Error setting file descriptor events for event poll: ", getLastErrorMessage());
    }
}

void EventPoll::unsubscribe(const int fileDescriptor)
{
    const auto event = nullptr;

    const auto unsubscribeResult = ::epoll_ctl(eventPollFileDescriptor_, EPOLL_CTL_DEL, fileDescriptor, event);

    if (unsubscribeResult == -1)
    {
        LOG_ERROR("Error unsubscribing file descriptor from event poll: ", getLastErrorMessage());
    }
}

int EventPoll::wait(std::vector<::epoll_event>& events)
{
    const auto timeout = -1;

    const auto numberOfPendingEvents =
        ::epoll_wait(eventPollFileDescriptor_, events.data(), static_cast<int>(events.size()), timeout);

    if (numberOfPendingEvents == -1)
    {
        WTHROW(InternalSocketError, "Error waiting for event poll: ", getLastErrorMessage());
    }

    return numberOfPendingEvents;
}

}
#endif
