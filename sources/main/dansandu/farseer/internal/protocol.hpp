#pragma once

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

uint64_t getNumberOfBits(const TypeEnum typeEnum);

class Type
{
public:
    static Type fromSimple(const TypeEnum typeEnum);

    static Type fromMessage(const std::string& identifier, bool hasStaticSize, uint64_t numberOfBits);

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

    uint64_t getNumberOfBits() const;

private:
    TypeEnum typeEnum_;
    std::string identifier_;
    std::unique_ptr<Type> subtype_;
    bool hasStaticSize_;
    uint64_t numberOfBits_;
};

struct Field
{
    uint32_t getHashCode() const;

    Type type;
    std::string identifier;
    bool hasStaticSize;
    uint64_t numberOfBits;
};

struct MessageProtocol
{
    uint32_t getHashCode() const;

    std::string identifier;
    std::vector<Field> fields;
    bool hasStaticSize;
    uint64_t numberOfBits;
};

struct RequestProtocol
{
    uint32_t getHashCode() const;

    std::string identifier;
    std::vector<Field> requestFields;
    std::vector<Field> responseFields;
};

struct Protocol
{
    std::string toString() const;

    std::string fileNamespace;
    std::vector<MessageProtocol> messages;
    std::vector<RequestProtocol> requests;
};

}
