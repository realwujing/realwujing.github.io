#include "logger.h"
#include "appender.h"
#include "console_appender.h"
#include <iostream>

int main()
{
    Logger l;
    l.addAppender(new ConsoleAppender());
    int i = 3000;
    while (i--)
    {
        l.log("INFO", "hello log console %d", i);
    }
    return 0;
}