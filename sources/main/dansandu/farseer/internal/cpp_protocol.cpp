#include "dansandu/farseer/internal/cpp_protocol.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/ballotin/string.hpp"

#include <sstream>

using dansandu::ballotin::string::join;
using dansandu::ballotin::string::split;
using dansandu::farseer::internal::protocol::Protocol;

namespace dansandu::farseer::internal::cpp_protocol
{

std::string generateCppProtocol(const Protocol& protocol)
{
    auto stream = std::ostringstream{};

    const auto cppNamespace = join(split(protocol.fileNamespace, "."), "::");

    stream << "#include <cstdint>\n"
           << "#include <string>\n"
           << "#include <vector>\n\n"
           << "namespace " << cppNamespace << "\n{\n\n";

    for (auto messagePosition = protocol.messages.cbegin(); messagePosition != protocol.messages.cend();
         ++messagePosition)
    {
        stream << "struct " << messagePosition->identifier << "\n{\n";

        for (const auto& field : messagePosition->fields)
        {
            stream << "    " << field.type.getCppType() << " " << field.identifier << ";\n";
        }

        stream << "};\n";

        if (messagePosition + 1 != protocol.messages.cend())
        {
            stream << std::endl;
        }
    }

    if (!protocol.messages.empty() && !protocol.requests.empty())
    {
        stream << std::endl;
    }

    for (auto requestPosition = protocol.requests.cbegin(); requestPosition != protocol.requests.cend();
         ++requestPosition)
    {
        stream << "struct " << requestPosition->identifier << "\n{\n";

        for (const auto& field : requestPosition->requestFields)
        {
            stream << "    " << field.type.getCppType() << " " << field.identifier << ";\n";
        }

        stream << "\n    struct response\n    {\n";

        for (const auto& field : requestPosition->responseFields)
        {
            stream << "        " << field.type.getCppType() << " " << field.identifier << ";\n";
        }

        stream << "    };\n};\n";

        if (requestPosition + 1 != protocol.requests.cend())
        {
            stream << std::endl;
        }
    }

    stream << "\n}\n";

    return stream.str();
}

}
