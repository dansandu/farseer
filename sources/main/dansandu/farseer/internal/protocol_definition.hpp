#pragma once

#include <memory>
#include <string>
#include <vector>

namespace dansandu::farseer::internal::protocol_definition
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
    custom,
};

const char* toString(const TypeEnum typeEnum);

class Type
{
public:
    static Type fromSimple(const TypeEnum typeEnum);

    static Type fromCustom(const std::string& identifier);

    static Type fromList(Type subtype);

    Type();

    Type(const Type& other);

    Type(Type&& other) noexcept = default;

    Type& operator=(const Type& other);

    Type& operator=(Type&& other) = default;

    TypeEnum getTypeEnum() const;

    std::string getIdentifier() const;

    const Type* getSubtype() const;

    std::string getCppType() const;

    std::string toString() const;

private:
    TypeEnum typeEnum_;
    std::string identifier_;
    std::unique_ptr<Type> subtype_;
};

struct Field
{
    Type type;
    std::string identifier;
};

struct MessageProtocol
{
    std::string identifier;
    std::vector<Field> fields;
};

struct RequestProtocol
{
    std::string identifier;
    std::vector<Field> requestFields;
    std::vector<Field> responseFields;
};

struct ProtocolFile
{
    std::string toString() const;

    std::string fileNamespace;
    std::vector<MessageProtocol> messages;
    std::vector<RequestProtocol> requests;
};

}
