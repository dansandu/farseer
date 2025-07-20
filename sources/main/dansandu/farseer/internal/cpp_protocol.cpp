#include "dansandu/farseer/internal/cpp_protocol.hpp"
#include "dansandu/ballotin/exception.hpp"
#include "dansandu/ballotin/string.hpp"

#include <sstream>

using dansandu::ballotin::string::join;
using dansandu::ballotin::string::split;
using dansandu::farseer::internal::protocol_definition::ProtocolFile;

namespace dansandu::farseer::internal::cpp_protocol
{

std::string generateCppProtocol(const ProtocolFile& protocolFile)
{
    auto stream = std::ostringstream{};

    const auto cppNamespace = join(split(protocolFile.fileNamespace, "."), "::");

    stream << "#include <cstdint>\n"
           << "#include <string>\n"
           << "#include <vector>\n\n"
           << "namespace " << cppNamespace << "\n{\n\n";

    for (auto messagePosition = protocolFile.messages.cbegin(); messagePosition != protocolFile.messages.cend();
         ++messagePosition)
    {
        stream << "struct " << messagePosition->identifier << "\n{\n";

        for (const auto& field : messagePosition->fields)
        {
            stream << "    " << field.type.getCppType() << " " << field.identifier << ";\n";
        }

        stream << "};\n";

        if (messagePosition + 1 != protocolFile.messages.cend())
        {
            stream << std::endl;
        }
    }

    if (!protocolFile.messages.empty() && !protocolFile.requests.empty())
    {
        stream << std::endl;
    }

    for (auto requestPosition = protocolFile.requests.cbegin(); requestPosition != protocolFile.requests.cend();
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

        if (requestPosition + 1 != protocolFile.requests.cend())
        {
            stream << std::endl;
        }
    }

    stream << "\n}\n";

    return stream.str();
}

}
