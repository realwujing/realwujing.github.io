
#include "console_appender.h"
#include <iostream>

void ConsoleAppender::append(const std::string &line)
{
    std::cout << line;
}
