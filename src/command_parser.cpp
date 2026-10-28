#include "command_parser.h"

#include <regex>
#include <string>
#include <sstream>
#include <vector>

namespace
{

constexpr const char* REGEX_NO_PARAMETER =
    R"(^\s*$)";

constexpr const char* REGEX_INTEGER =
    R"([+-]?[0-9]+)";

constexpr const char* REGEX_NUMBER =
    R"([+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))";

constexpr const char* REGEX_INS =
    R"(^\s*(0|1)\s*$)";

constexpr const char* REGEX_SAH =
    R"(^\s*(?:A|M(?:0[1-9]|1[0-6])(?:\s+M(?:0[1-9]|1[0-6]))*)\s*$)";

constexpr const char* REGEX_SHC =
    R"(^\s*M(?:0[1-9]|1[0-6])(?:\s+\S+)*\s*$)";

constexpr const char* REGEX_SHC_QUERY =
    R"(^\s*M(?:0[1-9]|1[0-6])\s*$)";

constexpr const char* REGEX_VLS =
    R"(^\s*[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s*$)";

constexpr const char* REGEX_SPI =
    R"(^\s*[RL]\s+[UVW]\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s*$)";

constexpr const char* REGEX_ADC_QUERY =
    R"(^\s*(?:[1-9][0-9]*)(?:\s+[1-9][0-9]*)*\s*$)";

constexpr const char* REGEX_SAC =
    R"(^\s*[1-9][0-9]*\s*$)";

constexpr const char* REGEX_BKN =
    R"(^\s*[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s*$)";

constexpr const char* REGEX_WRD =
    R"(^\s*[1-9][0-9]*\s+[1-9][0-9]*\s+[1-9][0-9]*\s+(1|2|4)\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s*$)";

constexpr const char* REGEX_RRD =
    R"(^\s*[1-9][0-9]*\s+[1-9][0-9]*\s+[1-9][0-9]*\s+(1|2|4)\s*$)";

constexpr const char* REGEX_MOV =
    R"(^\s*(?:R|L|PR|RP|LP)(?:\s+)[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:\s+)[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:\s+)[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:\s+)[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:\s+)[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:\s+)[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s*$)";

constexpr const char* REGEX_MRV =
    R"(^\s*(?:(?:R|L|RP|PR|LP)\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)|C\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))\s*$)";

constexpr const char* REGEX_MSV =
    R"(^\s*M(?:0[1-9]|1[0-6])\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s*$)";

constexpr const char* REGEX_MSR =
    R"(^\s*M(?:0[1-9]|1[0-6])\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\s*$)";

constexpr const char* REGEX_MPV =
    R"(^\s*(?:(?:M(?:0[1-9]|1[0-6])\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))(?:\s+M(?:0[1-9]|1[0-6])\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))*|M(?:0[1-9]|1[0-6])(?:\s+M(?:0[1-9]|1[0-6]))*\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))*)\s*$)";

constexpr const char* REGEX_MPR =
    R"(^\s*(?:(?:M(?:0[1-9]|1[0-6])\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))(?:\s+M(?:0[1-9]|1[0-6])\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))*|M(?:0[1-9]|1[0-6])(?:\s+M(?:0[1-9]|1[0-6]))*\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))*)\s*$)";

constexpr const char* REGEX_DFRS =
    R"(^\s*[^\\/\s]+\s*$)";

constexpr const char* REGEX_FDR =
    R"(^\s*[^\\/\s]+\s+[RL]\s+[XYZ]\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:\s+[XYZ]\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?(?:\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?(?:\s+V\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?(?:\s+TH\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?(?:\s+TT\s+[01])?(?:\s+ST\s+[012])?(?:\s+MP1\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?(?:\s+MP2\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?\s*$)";

constexpr const char* REGEX_FSM =
    R"(^\s*[^\\/\s]+\s+[RL]\s+[XYZ]\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:\s+[XYZ]\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?(?:\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?(?:\s+TH\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?(?:\s+S\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?(?:\s+V\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?\s*$)";

constexpr const char* REGEX_FRS =
    R"(^\s*[^\\/\s]+\s*$)";

constexpr const char* REGEX_FLM =
    R"(^\s*[^\\/\s]+\s+[XYZ]\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:\s+TH\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?(?:\s+V\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?\s*$)";

constexpr const char* REGEX_FDG =
    R"(^\s*[^\\/\s]+\s+[RL]\s+[XYZ]\s+[XYZ](?:\s+TH\s+[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))?\s*$)";

struct CommandRule
{
    const char* command;
    const char* parameterRegex;
};

static const CommandRule COMMAND_RULES[] =
{
    {"INS",   REGEX_INS},
    {"STP",   REGEX_NO_PARAMETER},
    {"SAH",   REGEX_SAH},
    {"SHC",   REGEX_SHC},
    {"SHC?",  REGEX_SHC_QUERY},
    {"SVO",   REGEX_NO_PARAMETER},
    {"SVF",   REGEX_NO_PARAMETER},
    {"CAL",   REGEX_NO_PARAMETER},
    {"ERR?",  REGEX_NO_PARAMETER},
    {"DSC",   REGEX_NO_PARAMETER},
    {"VLS",   REGEX_VLS},
    {"VLS?",  REGEX_NO_PARAMETER},
    {"MOV",   REGEX_MOV},
    {"MRV",   REGEX_MRV},
    {"MSV",   REGEX_MSV},
    {"MSR",   REGEX_MSR},
    {"SSI", REGEX_NO_PARAMETER}, 
    {"SSP", REGEX_NO_PARAMETER}, 
    {"SLI", REGEX_NO_PARAMETER},
    {"DBG", REGEX_NO_PARAMETER},
    {"APOS?", REGEX_NO_PARAMETER},
    {"MPV",   REGEX_MPV},
    {"MPR",   REGEX_MPR},
    {"MOV?",  REGEX_NO_PARAMETER},
    {"POS?",  REGEX_NO_PARAMETER},
    {"PMS?",  REGEX_NO_PARAMETER},
    {"SPI",   REGEX_SPI},
    {"SPI?",  REGEX_NO_PARAMETER},
    {"FRS",   REGEX_FRS},
    {"FRS?",  REGEX_NO_PARAMETER},
    {"DFRS",  REGEX_DFRS},
    {"FDR",   REGEX_FDR},
    {"FSM",   REGEX_FSM},
    {"FLM",   REGEX_FLM},
    {"FDG",   REGEX_FDG},
    {"ADC?",  REGEX_ADC_QUERY},
    {"SAC",   REGEX_SAC},
    {"BKN",   REGEX_BKN},
    {"BKN?",  REGEX_NO_PARAMETER},
    {"WRD",   REGEX_WRD},
    {"RRD",   REGEX_RRD}
};

constexpr std::size_t COMMAND_RULE_COUNT =
    sizeof(COMMAND_RULES) / sizeof(COMMAND_RULES[0]);

} // namespace

CommandParser::CommandParser()
{
}

ParseResult CommandParser::parse(const std::string& input) const
{   
    
    std::string command;
    std::string parameters;

    const std::size_t first = input.find_first_not_of(" \t\r\n");

    if (first == std::string::npos)
    {
        return {
            ParserResult::INVALID,
            "",
            "",
            "Empty command"
        };
    }

    const std::size_t commandEnd =
        input.find_first_of(" \t\r\n", first);

    if (commandEnd == std::string::npos)
    {
        command = input.substr(first);
    }
    else
    {
        command = input.substr(first, commandEnd - first);

        const std::size_t parameterStart =
            input.find_first_not_of(" \t\r\n", commandEnd);

        if (parameterStart != std::string::npos)
        {
            parameters = input.substr(parameterStart);
        }
    }

    for (std::size_t i = 0; i < COMMAND_RULE_COUNT; ++i)
    {
        const CommandRule& rule = COMMAND_RULES[i];

        if (command != rule.command)
        {
            continue;
        }

        if (rule.parameterRegex == nullptr)
        {
            return {
                ParserResult::INVALID,
                command,
                "",
                "Parameter rule not implemented"
            };
        }

        const std::regex regex(rule.parameterRegex);

        if (!std::regex_match(parameters, regex))
        {
            return {
                ParserResult::INVALID,
                command,
                "",
                "Invalid parameters\r\n"
            };
        }

        if (command == "MPV" || command == "MPR")
        {
            std::istringstream stream(parameters);
            std::vector<std::string> tokens;
            std::string token;

            while (stream >> token)
            {
                tokens.push_back(token);
            }

            std::size_t axisCount = 0;
            std::size_t valueCount = 0;

            if (tokens.size() >= 2 &&
                tokens[0][0] == 'M' &&
                tokens[1][0] != 'M')
            {
                // Mn Pn Mn Pn ...
                if ((tokens.size() % 2U) != 0U)
                {
                    return {
                        ParserResult::INVALID,
                        command,
                        "",
                        "Invalid parameters\r\n"
                    };
                }

                axisCount = tokens.size() / 2U;
                valueCount = tokens.size() / 2U;
            }
            else
            {
                // Mn Mn ... Pn Pn ...
                std::size_t index = 0;

                while (index < tokens.size() &&
                    tokens[index][0] == 'M')
                {
                    ++axisCount;
                    ++index;
                }

                valueCount = tokens.size() - index;
            }

            if (axisCount == 0 || axisCount != valueCount)
            {
                return {
                    ParserResult::INVALID,
                    command,
                    "",
                    "Invalid parameters\r\n"
                };
            }
        }

        return {
            ParserResult::VALID,
            command,
            input,
            ""
        };
    }

    return {
        ParserResult::INVALID,
        command,
        "",
        "Invalid command format.\r\n"
    };
}