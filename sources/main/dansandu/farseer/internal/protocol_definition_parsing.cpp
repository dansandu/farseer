#include "dansandu/farseer/internal/protocol_definition_parsing.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/protocol_validation.hpp"
#include "dansandu/glyph/parser.hpp"
#include "dansandu/glyph/regex_tokenizer.hpp"
#include "dansandu/glyph/symbol.hpp"
#include "dansandu/glyph/token.hpp"

#include <algorithm>
#include <numeric>
#include <set>
#include <string_view>
#include <vector>

using dansandu::farseer::exception::MessageNameNotDefinedError;
using dansandu::farseer::exception::ReservedNameError;
using dansandu::farseer::internal::protocol_definition::FieldDefinition;
using dansandu::farseer::internal::protocol_definition::MessageProtocolDefinition;
using dansandu::farseer::internal::protocol_definition::ProtocolDefinition;
using dansandu::farseer::internal::protocol_definition::RequestProtocolDefinition;
using dansandu::farseer::internal::protocol_definition::TypeDefinition;
using dansandu::farseer::internal::protocol_definition::TypeDefinitionEnum;
using dansandu::farseer::internal::protocol_validation::validateProtocolDefinition;
using dansandu::glyph::node::Node;
using dansandu::glyph::parser::Parser;
using dansandu::glyph::regex_tokenizer::RegexTokenizer;
using dansandu::glyph::symbol::Symbol;
using dansandu::glyph::token::Token;

namespace dansandu::farseer::internal::protocol_definition_parsing
{

namespace
{

constexpr auto protocolGrammar = R"(
    /* 0*/ Start -> ProtocolDefinition
    /* 1*/ ProtocolDefinition -> NamespaceDefinition Protocols
    /* 2*/ NamespaceDefinition -> namespace module semicolon
    /* 3*/ Protocols -> Protocols MessageDefinition
    /* 4*/ Protocols -> Protocols RequestDefinition
    /* 5*/ Protocols ->
    /* 6*/ MessageDefinition -> message name bracesBegin Fields bracesEnd
    /* 7*/ RequestDefinition -> request name bracesBegin RequestFields response bracesBegin Fields bracesEnd bracesEnd
    /* 8*/ RequestFields -> Fields
    /* 9*/ Fields -> Fields Type name semicolon
    /*10*/ Fields ->
    /*11*/ Type -> int32
    /*12*/ Type -> int64
    /*13*/ Type -> uint32
    /*14*/ Type -> uint64
    /*15*/ Type -> string
    /*16*/ Type -> bool
    /*17*/ Type -> list angleBracketBegin Type angleBracketEnd
    /*18*/ Type -> name
)";

// clang-format off
struct ProtocolDefinitionParser
{
    ProtocolDefinitionParser()
        : parser{protocolGrammar},
          moduleSymbol{parser.getTerminalSymbol("module")},
          nameSymbol{parser.getTerminalSymbol("name")},
          tokenizer{{
            {parser.getDiscardedSymbolPlaceholder(),        "\\s+"},
            {parser.getTerminalSymbol("semicolon"),         "\\;"},
            {parser.getTerminalSymbol("bracesBegin"),       "\\{"},
            {parser.getTerminalSymbol("bracesEnd"),         "\\}"},
            {parser.getTerminalSymbol("angleBracketBegin"), "\\<"},
            {parser.getTerminalSymbol("angleBracketEnd"),   "\\>"},
            {moduleSymbol,                                  "(\\b[a-zA-Z]\\w*\\b\\.)+\\b[a-zA-Z]\\w*\\b"},
            {parser.getTerminalSymbol("namespace"),         "\\bnamespace\\b"},
            {parser.getTerminalSymbol("message"),           "\\bmessage\\b"},
            {parser.getTerminalSymbol("request"),           "\\brequest\\b"},
            {parser.getTerminalSymbol("response"),          "\\bresponse\\b"},
            {parser.getTerminalSymbol("int32"),             "\\bint32\\b"},
            {parser.getTerminalSymbol("int64"),             "\\bint64\\b"},
            {parser.getTerminalSymbol("uint32"),            "\\buint32\\b"},
            {parser.getTerminalSymbol("uint64"),            "\\buint64\\b"},
            {parser.getTerminalSymbol("string"),            "\\bstring\\b"},
            {parser.getTerminalSymbol("bool"),              "\\bbool\\b"},
            {parser.getTerminalSymbol("list"),              "\\blist\\b"},
            {nameSymbol,                                    "\\b[a-zA-Z]\\w*\\b"},
          }}
    {
    }

    auto parse(const std::string_view text) const
    {
        return parser.parse(text, tokenizer);
    }

    Parser parser;
    Symbol moduleSymbol;
    Symbol nameSymbol;
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

ProtocolDefinition parseProtocolDefinition(const std::string_view text)
{
    static const ProtocolDefinitionParser parser;
    static const std::set<std::string> reservedIdentifierNames = {
        "Response", "static",  "module", "namespace", "template", "typename",  "if",        "else",
        "switch",   "while",   "for",    "class",     "struct",   "char",      "short",     "unsigned",
        "int",      "long",    "float",  "double",    "const",    "constexpr", "consteval", "this",
        "decltype", "default", "delete", "Metadata",  "auto",     "std",
    };

    const auto nodes = parser.parse(text);

    auto stack = std::vector<Token>{};

    auto type = TypeDefinition{};

    auto fields = std::vector<FieldDefinition>{};

    auto requestFields = std::vector<FieldDefinition>{};

    auto protocol = ProtocolDefinition{};

    const auto getTokenText = [text](const auto& token)
    { return std::string(text.cbegin() + token.begin(), text.cbegin() + token.end()); };

    for (const auto& node : nodes)
    {
        if (node.isToken())
        {
            const auto token = node.getToken();

            if (token.getSymbol() == parser.moduleSymbol || token.getSymbol() == parser.nameSymbol)
            {
                if (token.getSymbol() == parser.nameSymbol && reservedIdentifierNames.contains(getTokenText(token)))
                {
                    THROW(ReservedNameError, "the name '", getTokenText(token), "' is a reserved");
                }

                stack.push_back(token);
            }
        }
        else
        {
            const auto numberOfBitsAccumulator = [](const auto total, const auto& field)
            { return total + field.staticNumberOfBits; };

            switch (node.getRuleIndex())
            {
            case 2:
            {
                const auto token = pop(stack);
                protocol.fileNamespace = getTokenText(token);
                break;
            }
            case 6:
            {
                const auto token = pop(stack);
                const auto hasStaticSize =
                    std::all_of(fields.cbegin(), fields.cend(), [](const auto& field) { return field.hasStaticSize; });
                const auto staticNumberOfBits =
                    std::accumulate(fields.cbegin(), fields.cend(), ProtocolSize{}, numberOfBitsAccumulator);
                protocol.messages.push_back(MessageProtocolDefinition{.fileNamespace = protocol.fileNamespace,
                                                                      .name = getTokenText(token),
                                                                      .fields = std::move(fields),
                                                                      .hasStaticSize = hasStaticSize,
                                                                      .staticNumberOfBits = staticNumberOfBits});
                break;
            }
            case 7:
            {
                const auto token = pop(stack);
                const auto requestStaticNumberOfBits = std::accumulate(requestFields.cbegin(), requestFields.cend(),
                                                                       ProtocolSize{}, numberOfBitsAccumulator);
                const auto responseStaticNumberOfBits =
                    std::accumulate(fields.cbegin(), fields.cend(), ProtocolSize{}, numberOfBitsAccumulator);

                const auto selector = [](const auto& field) { return field.hasStaticSize; };
                const auto requestHasStaticSize = std::all_of(requestFields.cbegin(), requestFields.cend(), selector);
                const auto responseHasStaticSize = std::all_of(fields.cbegin(), fields.cend(), selector);

                protocol.requests.push_back(RequestProtocolDefinition{
                    .fileNamespace = protocol.fileNamespace,
                    .name = getTokenText(token),
                    .requestFields = std::move(requestFields),
                    .responseFields = std::move(fields),
                    .requestStaticNumberOfBits = requestStaticNumberOfBits,
                    .responseStaticNumberOfBits = responseStaticNumberOfBits,
                    .requestHasStaticSize = requestHasStaticSize,
                    .responseHasStaticSize = responseHasStaticSize,
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
                const auto hasStaticSize = type.hasStaticSize();
                const auto staticNumberOfBits = type.getStaticNumberOfBits();
                fields.push_back(FieldDefinition{
                    .type = std::move(type),
                    .name = getTokenText(token),
                    .hasStaticSize = hasStaticSize,
                    .staticNumberOfBits = staticNumberOfBits,
                });
                break;
            }
            case 11:
            {
                type = TypeDefinition::fromSimple(TypeDefinitionEnum::int32);
                break;
            }
            case 12:
            {
                type = TypeDefinition::fromSimple(TypeDefinitionEnum::int64);
                break;
            }
            case 13:
            {
                type = TypeDefinition::fromSimple(TypeDefinitionEnum::uint32);
                break;
            }
            case 14:
            {
                type = TypeDefinition::fromSimple(TypeDefinitionEnum::uint64);
                break;
            }
            case 15:
            {
                type = TypeDefinition::fromSimple(TypeDefinitionEnum::string);
                break;
            }
            case 16:
            {
                type = TypeDefinition::fromSimple(TypeDefinitionEnum::boolean);
                break;
            }
            case 17:
            {
                type = TypeDefinition::fromList(std::move(type));
                break;
            }
            case 18:
            {
                const auto token = pop(stack);
                const auto referencedMessageName = getTokenText(token);
                const auto referencedMessage = std::find_if(protocol.messages.cbegin(), protocol.messages.cend(),
                                                            [&referencedMessageName](const auto& message)
                                                            { return message.name == referencedMessageName; });

                if (referencedMessage == protocol.messages.cend())
                {
                    THROW(MessageNameNotDefinedError, "message '", referencedMessageName, "' was not defined");
                }

                type = TypeDefinition::fromMessage(referencedMessageName, referencedMessage->hasStaticSize,
                                                   referencedMessage->staticNumberOfBits);
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

    validateProtocolDefinition(protocol);

    return protocol;
}

}
