#include "appender.h"

class ConsoleAppender : public Appender
{
public:
    virtual ~ConsoleAppender() {}
    virtual void append(const std::string &line);
};