#pragma once

#include "dansandu/farseer/common.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dansandu::farseer::internal::protocol_definition
{

enum class Type
{
    i32,
    i64,
    u32,
    u64,
    string,
    boolean,
    list,
    map,
    message,
};

const char* toString(const Type type);

ProtocolSize getStaticNumberOfBits(const Type type);

class TypeDefinition
{
public:
    static TypeDefinition fromSimple(const Type type);

    static TypeDefinition fromMessage(const std::string& name, const bool hasStaticSize,
                                      const ProtocolSize staticNumberOfBits);

    static TypeDefinition fromList(TypeDefinition subtype);

    static TypeDefinition fromMap(TypeDefinition key, TypeDefinition value);

    TypeDefinition(const TypeDefinition& other);

    TypeDefinition(TypeDefinition&& other) noexcept;

    TypeDefinition& operator=(const TypeDefinition& other);

    TypeDefinition& operator=(TypeDefinition&& other) noexcept;

    Type getType() const;

    const std::string& getName() const;

    const std::vector<TypeDefinition>& getSubtypes() const;

    std::string getCppType() const;

    std::string toString() const;

    uint32_t getHashCode() const;

    bool hasStaticSize() const;

    ProtocolSize getStaticNumberOfBits() const;

    bool canBeMapKey() const;

private:
    TypeDefinition();

    Type type_;
    std::string name_;
    std::vector<TypeDefinition> subtypes_;
    bool hasStaticSize_;
    ProtocolSize staticNumberOfBits_;
};

struct FieldDefinition
{
    uint32_t getHashCode() const;

    bool hasStaticSize() const;

    ProtocolSize getStaticNumberOfBits() const;

    TypeDefinition typeDefinition;
    std::string name;
};

struct MessageProtocolDefinition
{
    uint32_t getHashCode() const;

    bool hasStaticSize() const;

    ProtocolSize getStaticNumberOfBits() const;

    std::string fileNamespace;
    std::string name;
    std::vector<FieldDefinition> fields;
};

struct RequestProtocolDefinition
{
    uint32_t getRequestHashCode() const;

    uint32_t getResponseHashCode() const;

    bool requestHasStaticSize() const;

    bool responseHasStaticSize() const;

    ProtocolSize getRequestStaticNumberOfBits() const;

    ProtocolSize getResponseStaticNumberOfBits() const;

    std::string fileNamespace;
    std::string name;
    std::vector<FieldDefinition> requestFields;
    std::vector<FieldDefinition> responseFields;
};

struct ProtocolDefinition
{
    std::string toString() const;

    std::string fileNamespace;
    std::vector<MessageProtocolDefinition> messages;
    std::vector<RequestProtocolDefinition> requests;
};

}
