#include "dansandu/farseer/internal/protocol_parsing.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/glyph/parser.hpp"
#include "dansandu/glyph/regex_tokenizer.hpp"
#include "dansandu/glyph/symbol.hpp"
#include "dansandu/glyph/token.hpp"

#include <string_view>
#include <vector>

using dansandu::farseer::internal::protocol_definition::Field;
using dansandu::farseer::internal::protocol_definition::MessageProtocol;
using dansandu::farseer::internal::protocol_definition::ProtocolFile;
using dansandu::farseer::internal::protocol_definition::RequestProtocol;
using dansandu::farseer::internal::protocol_definition::Type;
using dansandu::farseer::internal::protocol_definition::TypeEnum;
using dansandu::glyph::node::Node;
using dansandu::glyph::parser::Parser;
using dansandu::glyph::regex_tokenizer::RegexTokenizer;
using dansandu::glyph::symbol::Symbol;
using dansandu::glyph::token::Token;

namespace dansandu::farseer::internal::protocol_parsing
{

namespace
{

constexpr auto protocolsGrammar = R"(
    /* 0*/ Start -> ProtocolFile
    /* 1*/ ProtocolFile -> NamespaceDefinition ProtocolDefinitions
    /* 2*/ NamespaceDefinition -> namespace module semicolon
    /* 3*/ ProtocolDefinitions -> ProtocolDefinitions MessageDefinition
    /* 4*/ ProtocolDefinitions -> ProtocolDefinitions RequestDefinition
    /* 5*/ ProtocolDefinitions -> 
    /* 6*/ MessageDefinition -> message identifier bracesBegin Fields bracesEnd
    /* 7*/ RequestDefinition -> request identifier bracesBegin RequestFields response bracesBegin Fields bracesEnd bracesEnd
    /* 8*/ RequestFields -> Fields
    /* 9*/ Fields -> Fields Type identifier semicolon
    /*10*/ Fields -> 
    /*11*/ Type -> int32
    /*12*/ Type -> int64
    /*13*/ Type -> uint32
    /*14*/ Type -> uint64
    /*15*/ Type -> string
    /*16*/ Type -> boolean
    /*17*/ Type -> list angleBracketBegin Type angleBracketEnd
    /*18*/ Type -> identifier
)";

// clang-format off
struct ProtocolParser
{
    ProtocolParser()
        : parser{protocolsGrammar},
          moduleSymbol{parser.getTerminalSymbol("module")},
          identifier{parser.getTerminalSymbol("identifier")},
          tokenizer{{
            {parser.getDiscardedSymbolPlaceholder(),        "\\s+"},
            {parser.getTerminalSymbol("semicolon"),         "\\;"},
            {parser.getTerminalSymbol("bracesBegin"),       "\\{"},
            {parser.getTerminalSymbol("bracesEnd"),         "\\}"},
            {parser.getTerminalSymbol("angleBracketBegin"), "\\<"},
            {parser.getTerminalSymbol("angleBracketEnd"),   "\\>"},
            {moduleSymbol,                                  "([a-zA-Z]\\w*\\.)+[a-zA-Z]\\w*"},
            {parser.getTerminalSymbol("namespace"),         "namespace"},
            {parser.getTerminalSymbol("message"),           "message"},
            {parser.getTerminalSymbol("request"),           "request"},
            {parser.getTerminalSymbol("response"),          "response"},
            {parser.getTerminalSymbol("int32"),             "int32"},
            {parser.getTerminalSymbol("int64"),             "int64"},
            {parser.getTerminalSymbol("uint32"),            "uint32"},
            {parser.getTerminalSymbol("uint64"),            "uint64"},
            {parser.getTerminalSymbol("string"),            "string"},
            {parser.getTerminalSymbol("boolean"),           "boolean"},
            {parser.getTerminalSymbol("list"),              "list"},
            {identifier,                                    "[a-zA-Z]\\w*"},
          }}
    {
    }

    auto parse(const std::string_view text) const
    {
        return parser.parse(text, tokenizer);
    }

    Parser parser;
    Symbol moduleSymbol;
    Symbol identifier;
    RegexTokenizer tokenizer;
};
// clang-format on

template<typename T>
auto pop(std::vector<T>& stack)
{
    if (stack.empty())
    {
        THROW(std::logic_error, "cannot pop empty stack");
    }

    auto value = std::move(stack.back());
    stack.pop_back();
    return value;
}

}

ProtocolFile parseProtocolFile(const std::string_view text)
{
    static const ProtocolParser parser;

    const auto nodes = parser.parse(text);

    auto stack = std::vector<Token>{};

    auto type = Type{};

    auto fields = std::vector<Field>{};

    auto requestFields = std::vector<Field>{};

    auto protocolFile = ProtocolFile{};

    const auto getTokenText = [text](const auto& token)
    { return std::string(text.cbegin() + token.begin(), text.cbegin() + token.end()); };

    for (const auto& node : nodes)
    {
        if (node.isToken())
        {
            const auto token = node.getToken();

            if (token.getSymbol() == parser.moduleSymbol || token.getSymbol() == parser.identifier)
            {
                stack.push_back(token);
            }
        }
        else
        {
            switch (node.getRuleIndex())
            {
            case 2:
            {
                const auto token = pop(stack);
                protocolFile.fileNamespace = getTokenText(token);
                break;
            }
            case 6:
            {
                const auto token = pop(stack);
                protocolFile.messages.push_back(
                    MessageProtocol{.identifier = getTokenText(token), .fields = std::move(fields)});
                break;
            }
            case 7:
            {
                const auto token = pop(stack);
                protocolFile.requests.push_back(RequestProtocol{
                    .identifier = getTokenText(token),
                    .requestFields = std::move(requestFields),
                    .responseFields = std::move(fields),
                });
                break;
            }
            case 8:
            {
                requestFields = std::move(fields);
                break;
            }
            case 9:
            {
                const auto token = pop(stack);
                fields.push_back(Field{
                    .type = std::move(type),
                    .identifier = getTokenText(token),
                });
                break;
            }
            case 11:
            {
                type.typeEnum = TypeEnum::int32;
                type.identifier.clear();
                type.subtype.reset();
                break;
            }
            case 12:
            {
                type.typeEnum = TypeEnum::int64;
                type.identifier.clear();
                type.subtype.reset();
                break;
            }
            case 13:
            {
                type.typeEnum = TypeEnum::uint32;
                type.identifier.clear();
                type.subtype.reset();
                break;
            }
            case 14:
            {
                type.typeEnum = TypeEnum::uint64;
                type.identifier.clear();
                type.subtype.reset();
                break;
            }
            case 15:
            {
                type.typeEnum = TypeEnum::string;
                type.identifier.clear();
                type.subtype.reset();
                break;
            }
            case 16:
            {
                type.typeEnum = TypeEnum::boolean;
                type.identifier.clear();
                type.subtype.reset();
                break;
            }
            case 17:
            {
                auto subtype = std::move(type);
                type.typeEnum = TypeEnum::list;
                type.identifier.clear();
                type.subtype = std::make_unique<Type>(std::move(subtype));
                break;
            }
            case 18:
            {
                const auto token = pop(stack);
                type.typeEnum = TypeEnum::custom;
                type.identifier = getTokenText(token);
                type.subtype.reset();
                break;
            }
            case 0:
            case 1:
            case 3:
            case 4:
            case 5:
            case 10:
                break;
            default:
                THROW(std::logic_error, "production rule ", node.getRuleIndex(), " was not exhausted");
            }
        }
    }

    return protocolFile;
}

}
