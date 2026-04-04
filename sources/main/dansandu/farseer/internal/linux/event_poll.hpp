#pragma once

#include <sys/epoll.h>

#include <cstdint>
#include <vector>

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

    void subscribe(const int fileDescriptor, const uint32_t events);

    void modify(const int fileDescriptor, const uint32_t events);

    void unsubscribe(const int fileDescriptor);

    int wait(std::vector<::epoll_event>& events);

private:
    const int eventPollFileDescriptor_;
};

}
