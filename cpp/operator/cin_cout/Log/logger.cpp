#include "logger.h"

#include <time.h>
#include <sstream>
#include <fstream>
#include <iostream>

Logger::~Logger()
{
    for (std::vector<Appender *>::iterator it = mAppenderList.begin();
         it != mAppenderList.end();
         ++it)
    {
        delete *it;
    }
}

void Logger::addAppender(Appender *appender)
{
    mAppenderList.push_back(appender);
}

void Logger::log(const std::string &level, const std::string &str)
{
    writeLog(level, str);
}

void Logger::log(const char *level, const char *fmt, ...)
{
    memset(mLogStr, 0, MAX_LINE);
    va_list args;
    va_start(args, fmt);
    vsnprintf(mLogStr, sizeof(mLogStr), fmt, args);
    va_end(args);
    writeLog(level, mLogStr);
}

void Logger::writeLog(const std::string &level, const std::string &str)
{
    std::string line = format(level, str);
    for (std::vector<Appender *>::iterator it = mAppenderList.begin();
         it != mAppenderList.end();
         ++it)
    {
        (*it)->append(line);
    }
}

std::string Logger::format(const std::string &level, const std::string &str)
{
    std::ostringstream os;
    os << "[" << getTime() << "] " << level << " "
       << __func__ << " "
       << str << "\n";
    return os.str();
}

std::string Logger::getTime()
{
    char buf[20];
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    strftime(buf, sizeof(buf), "%F %X", local_time);
    return buf;
}