#pragma once

namespace dansandu::farseer::internal::linux::event_poll
{

class EventPoll
{
public:
    EventPoll(const EventPoll&) = delete;
    EventPoll(EventPoll&&) noexcept = delete;
    EventPoll& operator=(const EventPoll&) = delete;
    EventPoll& operator=(EventPoll&&) noexcept = delete;

    EventPoll();

    ~EventPoll() noexcept;

    int getEventPollFileDescriptor() const
    {
        return eventPollFileDescriptor_;
    }

private:
    const int eventPollFileDescriptor_;
};

}
