#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dansandu::farseer::internal::protocol_parsing
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

struct Type
{
    std::string toString() const;

    TypeEnum typeEnum;
    std::string identifier;
    std::unique_ptr<Type> subtype;
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

ProtocolFile parseProtocolFile(const std::string_view text);

}
