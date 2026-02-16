#pragma once

#include <atomic>

namespace dansandu::farseer::internal::sequencer
{

template<typename GeneratedType>
class Sequencer
{
public:
    using UnderlyingType = typename GeneratedType::UnderlyingType;

    Sequencer() : underlying_{0}
    {
    }

    explicit Sequencer(const UnderlyingType underlying) : underlying_{underlying}
    {
    }

    Sequencer(const Sequencer& other) = delete;
    Sequencer(Sequencer&& other) noexcept = delete;
    Sequencer& operator=(const Sequencer& other) = delete;
    Sequencer& operator=(Sequencer&& other) noexcept = delete;

    GeneratedType generate()
    {
        return GeneratedType{underlying_++};
    }

private:
    std::atomic<UnderlyingType> underlying_;
};

}
