#pragma once

#include <atomic>

namespace dansandu::farseer::internal::sequencer
{

template<typename GeneratedType>
class Sequencer
{
public:
    using IntegerType = typename GeneratedType::IntegerType;

    Sequencer() : integer_{0}
    {
    }

    explicit Sequencer(const IntegerType integer) : integer_{integer}
    {
    }

    Sequencer(const Sequencer& other) = delete;
    Sequencer(Sequencer&& other) noexcept = delete;
    Sequencer& operator=(const Sequencer& other) = delete;
    Sequencer& operator=(Sequencer&& other) noexcept = delete;

    GeneratedType generate()
    {
        return GeneratedType{integer_++};
    }

private:
    std::atomic<IntegerType> integer_;
};

}
