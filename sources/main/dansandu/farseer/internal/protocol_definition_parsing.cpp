#include "dansandu/farseer/internal/protocol_definition_parsing.hpp"
#include "dansandu/farseer/exception.hpp"
#include "dansandu/farseer/internal/protocol_validation.hpp"
#include "dansandu/glyph/parser.hpp"
#include "dansandu/glyph/regex_tokenizer.hpp"
#include "dansandu/glyph/symbol.hpp"
#include "dansandu/glyph/token.hpp"

#include <algorithm>
#include <set>
#include <string_view>
#include <vector>

using dansandu::farseer::exception::InvalidMapKeyError;
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
    /*18*/ Type -> map angleBracketBegin Type comma Type angleBracketEnd
    /*19*/ Type -> name
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
            {parser.getTerminalSymbol("comma"),             "\\,"},
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
            {parser.getTerminalSymbol("map"),               "\\bmap\\b"},
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
        "decltype", "default", "delete", "Metadata",  "auto",     "std",       "list",      "map",
    };

    const auto nodes = parser.parse(text);

    auto tokens = std::vector<Token>{};

    auto types = std::vector<TypeDefinition>{};

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

                tokens.push_back(token);
            }
        }
        else
        {
            switch (node.getRuleIndex())
            {
            case 2:
            {
                const auto token = pop(tokens);
                protocol.fileNamespace = getTokenText(token);
                break;
            }
            case 6:
            {
                protocol.messages.push_back(MessageProtocolDefinition{
                    .fileNamespace = protocol.fileNamespace,
                    .name = getTokenText(pop(tokens)),
                    .fields = std::move(fields),
                });
                break;
            }
            case 7:
            {
                protocol.requests.push_back(RequestProtocolDefinition{
                    .fileNamespace = protocol.fileNamespace,
                    .name = getTokenText(pop(tokens)),
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
                fields.push_back(FieldDefinition{
                    .type = pop(types),
                    .name = getTokenText(pop(tokens)),
                });
                break;
            }
            case 11:
            {
                types.push_back(TypeDefinition::fromSimple(TypeDefinitionEnum::int32));
                break;
            }
            case 12:
            {
                types.push_back(TypeDefinition::fromSimple(TypeDefinitionEnum::int64));
                break;
            }
            case 13:
            {
                types.push_back(TypeDefinition::fromSimple(TypeDefinitionEnum::uint32));
                break;
            }
            case 14:
            {
                types.push_back(TypeDefinition::fromSimple(TypeDefinitionEnum::uint64));
                break;
            }
            case 15:
            {
                types.push_back(TypeDefinition::fromSimple(TypeDefinitionEnum::string));
                break;
            }
            case 16:
            {
                types.push_back(TypeDefinition::fromSimple(TypeDefinitionEnum::boolean));
                break;
            }
            case 17:
            {
                types.push_back(TypeDefinition::fromList(pop(types)));
                break;
            }
            case 18:
            {
                auto value = pop(types);
                auto key = pop(types);

                if (!key.canBeMapKey())
                {
                    THROW(InvalidMapKeyError, "Type '", key.toString(), "' cannot be used as a map key");
                }

                types.push_back(TypeDefinition::fromMap(std::move(key), std::move(value)));
                break;
            }
            case 19:
            {
                const auto referencedMessageName = getTokenText(pop(tokens));
                const auto referencedMessage = std::find_if(protocol.messages.cbegin(), protocol.messages.cend(),
                                                            [&referencedMessageName](const auto& message)
                                                            { return message.name == referencedMessageName; });

                if (referencedMessage == protocol.messages.cend())
                {
                    THROW(MessageNameNotDefinedError, "message '", referencedMessageName, "' was not defined");
                }

                types.push_back(TypeDefinition::fromMessage(referencedMessageName, referencedMessage->hasStaticSize(),
                                                            referencedMessage->getStaticNumberOfBits()));
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
