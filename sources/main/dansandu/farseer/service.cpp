#include "dansandu/ballotin/file_system.hpp"
#include "dansandu/farseer/internal/cpp_protocol.hpp"
#include "dansandu/farseer/internal/protocol_definition_parsing.hpp"
#include "dansandu/service_runner/service_registry.hpp"

#include <iostream>

using dansandu::ballotin::file_system::readAsciiFile;
using dansandu::ballotin::file_system::writeAsciiFile;
using dansandu::farseer::internal::cpp_protocol::generateProtocolCppHeader;
using dansandu::farseer::internal::cpp_protocol::generateProtocolCppSource;
using dansandu::farseer::internal::protocol_definition_parsing::parseProtocolDefinition;

namespace dansandu::farseer::service
{

namespace
{

int generateProtocolSourceFiles(const int argumentsCount, const char* const* const arguments)
{
    if (argumentsCount == 0)
    {
        std::cout << R"(Generate C++ source from a given input protocol file.

    --protocol
        path to an existing protocol file

    --cpp-header
        file path for the generated C++ header

    --cpp-source
        file path for the generated C++ source
)";
        return 0;
    }

    if (argumentsCount < 3)
    {
        std::cerr << "Invalid arguments were provided." << std::endl;
        return 5;
    }

    const auto protocolArgument = std::string{"--protocol"};
    const auto cppHeaderArgument = std::string{"--cpp-header"};
    const auto cppSourceArgument = std::string{"--cpp-source"};

    auto protocolFilePath = std::string{};
    auto cppHeaderFilePath = std::string{};
    auto cppSourceFilePath = std::string{};

    auto index = 0;

    while (index < argumentsCount)
    {
        if (arguments[index] == protocolArgument)
        {
            if (index + 1 < argumentsCount)
            {
                protocolFilePath = arguments[index + 1];
                index += 2;
            }
            else
            {
                std::cerr << "The protocol file path was not provided." << std::endl;
                return 1;
            }
        }
        else if (arguments[index] == cppHeaderArgument)
        {
            if (index + 1 < argumentsCount)
            {
                cppHeaderFilePath = arguments[index + 1];
                index += 2;
            }
            else
            {
                std::cerr << "The C++ header file path was not provided." << std::endl;
                return 2;
            }
        }
        else if (arguments[index] == cppSourceArgument)
        {
            if (index + 1 < argumentsCount)
            {
                cppSourceFilePath = arguments[index + 1];
                index += 2;
            }
            else
            {
                std::cerr << "The C++ source file path was not provided." << std::endl;
                return 3;
            }
        }
        else
        {
            std::cerr << "Invalid arguments were provided." << std::endl;
            return 4;
        }
    }

    const auto protocolFile = readAsciiFile(protocolFilePath);
    const auto protocol = parseProtocolDefinition(protocolFile);
    const auto cppHeader = generateProtocolCppHeader(protocol);
    const auto cppSource = generateProtocolCppSource(protocol);

    writeAsciiFile(cppHeaderFilePath, cppHeader);
    writeAsciiFile(cppSourceFilePath, cppSource);

    return 0;
}

}

DANSANDU_SERVICE_RUNNER_REGISTER_SERVICE("dansandu-farseer-generate_protocol", generateProtocolSourceFiles);

}
