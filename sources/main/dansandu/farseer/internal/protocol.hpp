#pragma once

#include "dansandu/farseer/common.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dansandu::farseer::internal::protocol
{

enum class TypeEnum
{
    int32,
    int64,
    uint32,
    uint64,
    string,
    boolean,
    list,
    message,
};

const char* toString(const TypeEnum typeEnum);

ProtocolSize getStaticNumberOfBits(const TypeEnum typeEnum);

class Type
{
public:
    static Type fromSimple(const TypeEnum typeEnum);

    static Type fromMessage(const std::string& identifier, const bool hasStaticSize,
                            const ProtocolSize staticNumberOfBits);

    static Type fromList(Type subtype);

    Type();

    Type(const Type& other);

    Type(Type&& other) noexcept;

    Type& operator=(const Type& other);

    Type& operator=(Type&& other) noexcept;

    TypeEnum getTypeEnum() const;

    const std::string& getIdentifier() const;

    const Type* getSubtype() const;

    std::string getCppType() const;

    std::string toString() const;

    uint32_t getHashCode() const;

    bool hasStaticSize() const;

    ProtocolSize getStaticNumberOfBits() const;

private:
    TypeEnum typeEnum_;
    std::string identifier_;
    std::unique_ptr<Type> subtype_;
    bool hasStaticSize_;
    ProtocolSize staticNumberOfBits_;
};

struct Field
{
    uint32_t getHashCode() const;

    Type type;
    std::string identifier;
    bool hasStaticSize;
    ProtocolSize staticNumberOfBits;
};

struct MessageProtocol
{
    uint32_t getHashCode() const;

    std::string identifier;
    std::vector<Field> fields;
    bool hasStaticSize;
    ProtocolSize staticNumberOfBits;
};

struct RequestProtocol
{
    uint32_t getRequestHashCode() const;

    uint32_t getResponseHashCode() const;

    std::string identifier;
    std::vector<Field> requestFields;
    std::vector<Field> responseFields;
    ProtocolSize requestStaticNumberOfBits;
    ProtocolSize responseStaticNumberOfBits;
    bool requestHasStaticSize;
    bool responseHasStaticSize;
};

struct Protocol
{
    std::string toString() const;

    std::string fileNamespace;
    std::vector<MessageProtocol> messages;
    std::vector<RequestProtocol> requests;
};

}
