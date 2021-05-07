
#ifndef __LOG_APPENDER_H__
#define __LOG_APPENDER_H__

#include <string> 

class Appender
{
public:
    virtual ~Appender() {}
    virtual void append(const std::string &line) = 0;
};

#endif