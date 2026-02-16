#include "dansandu/farseer/internal/protocol_parsing.hpp"
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

using dansandu::farseer::exception::MessageIdentifierNotDefinedError;
using dansandu::farseer::exception::ReservedIdentifierNameError;
using dansandu::farseer::internal::protocol::Field;
using dansandu::farseer::internal::protocol::MessageProtocol;
using dansandu::farseer::internal::protocol::Protocol;
using dansandu::farseer::internal::protocol::RequestProtocol;
using dansandu::farseer::internal::protocol::Type;
using dansandu::farseer::internal::protocol::TypeEnum;
using dansandu::farseer::internal::protocol_validation::validateProtocol;
using dansandu::glyph::node::Node;
using dansandu::glyph::parser::Parser;
using dansandu::glyph::regex_tokenizer::RegexTokenizer;
using dansandu::glyph::symbol::Symbol;
using dansandu::glyph::token::Token;

namespace dansandu::farseer::internal::protocol_parsing
{

namespace
{

constexpr auto protocolGrammar = R"(
    /* 0*/ Start -> Protocol
    /* 1*/ Protocol -> NamespaceDefinition ProtocolDefinitions
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
    /*16*/ Type -> bool
    /*17*/ Type -> list angleBracketBegin Type angleBracketEnd
    /*18*/ Type -> identifier
)";

// clang-format off
struct ProtocolParser
{
    ProtocolParser()
        : parser{protocolGrammar},
          moduleSymbol{parser.getTerminalSymbol("module")},
          identifier{parser.getTerminalSymbol("identifier")},
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
            {identifier,                                    "\\b[a-zA-Z]\\w*\\b"},
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

Protocol parseProtocol(const std::string_view text)
{
    static const ProtocolParser parser;
    static const std::set<std::string> reservedIdentifierNames = {
        "Response", "static",  "module", "namespace", "template", "typename",  "if",        "else",
        "switch",   "while",   "for",    "class",     "struct",   "char",      "short",     "unsigned",
        "int",      "long",    "float",  "double",    "const",    "constexpr", "consteval", "this",
        "decltype", "default", "delete", "Metadata",  "auto",     "std",
    };

    const auto nodes = parser.parse(text);

    auto stack = std::vector<Token>{};

    auto type = Type{};

    auto fields = std::vector<Field>{};

    auto requestFields = std::vector<Field>{};

    auto protocol = Protocol{};

    const auto getTokenText = [text](const auto& token)
    { return std::string(text.cbegin() + token.begin(), text.cbegin() + token.end()); };

    for (const auto& node : nodes)
    {
        if (node.isToken())
        {
            const auto token = node.getToken();

            if (token.getSymbol() == parser.moduleSymbol || token.getSymbol() == parser.identifier)
            {
                if (token.getSymbol() == parser.identifier && reservedIdentifierNames.contains(getTokenText(token)))
                {
                    THROW(ReservedIdentifierNameError, "the identifier name '", getTokenText(token),
                          "' is a reserved name");
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
                protocol.messages.push_back(MessageProtocol{.identifier = getTokenText(token),
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

                protocol.requests.push_back(RequestProtocol{
                    .identifier = getTokenText(token),
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
                fields.push_back(Field{
                    .type = std::move(type),
                    .identifier = getTokenText(token),
                    .hasStaticSize = hasStaticSize,
                    .staticNumberOfBits = staticNumberOfBits,
                });
                break;
            }
            case 11:
            {
                type = Type::fromSimple(TypeEnum::int32);
                break;
            }
            case 12:
            {
                type = Type::fromSimple(TypeEnum::int64);
                break;
            }
            case 13:
            {
                type = Type::fromSimple(TypeEnum::uint32);
                break;
            }
            case 14:
            {
                type = Type::fromSimple(TypeEnum::uint64);
                break;
            }
            case 15:
            {
                type = Type::fromSimple(TypeEnum::string);
                break;
            }
            case 16:
            {
                type = Type::fromSimple(TypeEnum::boolean);
                break;
            }
            case 17:
            {
                type = Type::fromList(std::move(type));
                break;
            }
            case 18:
            {
                const auto token = pop(stack);
                const auto referencedMessageIdentifier = getTokenText(token);
                const auto referencedMessage =
                    std::find_if(protocol.messages.cbegin(), protocol.messages.cend(),
                                 [&referencedMessageIdentifier](const auto& message)
                                 { return message.identifier == referencedMessageIdentifier; });

                if (referencedMessage == protocol.messages.cend())
                {
                    THROW(MessageIdentifierNotDefinedError, "message '", referencedMessageIdentifier,
                          "' was not defined");
                }

                type = Type::fromMessage(referencedMessageIdentifier, referencedMessage->hasStaticSize,
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

    validateProtocol(protocol);

    return protocol;
}

}
