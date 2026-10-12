#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <string>

enum class ParserResult
{
    VALID,
    INVALID
};

struct ParseResult
{
    ParserResult result;
    std::string command;
    std::string payload;
    std::string error;
};

class CommandParser
{
public:
    CommandParser();

    ParseResult parse(const std::string& input) const;
};

#endif // COMMAND_PARSER_H