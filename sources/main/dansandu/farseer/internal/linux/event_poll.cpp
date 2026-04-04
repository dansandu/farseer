#if defined(__linux__)
#include "dansandu/farseer/internal/linux/event_poll.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/linux/error.hpp"
#include "dansandu/journey/logging.hpp"

#include <sys/epoll.h>
#include <unistd.h>

using dansandu::farseer::exception::InternalSocketError;
using dansandu::farseer::internal::linux::error::getLastErrorMessage;

namespace dansandu::farseer::internal::linux::event_poll
{

namespace
{

int createEventPollFileDescriptor()
{
    const auto flags = 0;
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

}
#endif
