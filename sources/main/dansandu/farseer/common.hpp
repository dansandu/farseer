#pragma once

#include "dansandu/ballotin/function.hpp"
#include "dansandu/ballotin/type_prototype.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/expected.hpp"

#include <any>
#include <cstdint>
#include <vector>

namespace dansandu::farseer
{

using dansandu::farseer::expected::Expected;

using dansandu::farseer::exception::RequestProtocolError;

using dansandu::ballotin::function::UniqueFunction;

class ProtocolIdentifierTag
{
};

using ProtocolIdentifier = dansandu::ballotin::type_prototype::TypePrototype<
    ProtocolIdentifierTag, uint32_t,
    dansandu::ballotin::type_prototype::TypeFeature::underlyingConversion |
        dansandu::ballotin::type_prototype::TypeFeature::stringConversion |
        dansandu::ballotin::type_prototype::TypeFeature::equality |
        dansandu::ballotin::type_prototype::TypeFeature::inequality>;

static_assert(sizeof(ProtocolIdentifier) == sizeof(typename ProtocolIdentifier::UnderlyingType),
              "Serialization requires that the ProtocolIdentifier size must match its underlying type size");

class ProtocolSizeTag
{
};

using ProtocolSize = dansandu::ballotin::type_prototype::TypePrototype<
    ProtocolSizeTag, uint32_t,
    dansandu::ballotin::type_prototype::TypeFeature::underlyingConversion |
        dansandu::ballotin::type_prototype::TypeFeature::stringConversion |
        dansandu::ballotin::type_prototype::TypeFeature::equality |
        dansandu::ballotin::type_prototype::TypeFeature::inequality |
        dansandu::ballotin::type_prototype::TypeFeature::addition>;

static_assert(sizeof(ProtocolSize) == sizeof(typename ProtocolSize::UnderlyingType),
              "Serialization requires that the ProtocolSize size must match its underlying type size");

class ProtocolSequenceNumberTag
{
};

using ProtocolSequenceNumber = dansandu::ballotin::type_prototype::TypePrototype<
    ProtocolSequenceNumberTag, uint64_t,
    dansandu::ballotin::type_prototype::TypeFeature::underlyingConversion |
        dansandu::ballotin::type_prototype::TypeFeature::stringConversion |
        dansandu::ballotin::type_prototype::TypeFeature::equality |
        dansandu::ballotin::type_prototype::TypeFeature::inequality>;

static_assert(sizeof(ProtocolSequenceNumber) == sizeof(typename ProtocolSequenceNumber::UnderlyingType),
              "Serialization requires that the ProtocolSequenceNumber size must match its underlying type size");

class SocketIdentifierTag
{
};

using SocketIdentifier = dansandu::ballotin::type_prototype::TypePrototype<
    SocketIdentifierTag, unsigned long,
    dansandu::ballotin::type_prototype::TypeFeature::underlyingConversion |
        dansandu::ballotin::type_prototype::TypeFeature::stringConversion |
        dansandu::ballotin::type_prototype::TypeFeature::equality |
        dansandu::ballotin::type_prototype::TypeFeature::inequality>;

static constexpr SocketIdentifier invalidSocketIdentifier = SocketIdentifier{};

enum class SocketEvent
{
    serverOpen,
    serverClosed,
    serverAborted,
    clientOpen,
    clientBytesReceived,
    clientBytesSent,
    clientClosed,
    clientAborted,
};

PRALINE_EXPORT ProtocolSize getProtocolSizeFromStdSize(const size_t size);

PRALINE_EXPORT const char* toString(const SocketEvent event);

using ConnectionCallback = UniqueFunction<void(const SocketEvent event, const SocketIdentifier identifier)>;

using ProtocolDeserializer = bool (*)(const std::vector<uint8_t>& bytes, size_t& bitsOffset,
                                      ProtocolSequenceNumber& sequenceNumber, std::any& protocol);

using ExpectedResponseProtocolSerializer = std::vector<uint8_t> (*)(const std::any& expectedResponse,
                                                                    const ProtocolSequenceNumber sequenceNumber);

}
