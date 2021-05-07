#include "log_stream.h"
#include "logger.h"
#include "console_appender.h"
#include "file_appender.h"
static Logger logger;

LogStream::LogStream(const std::string &level) : mLogLevel(level)
{
}

LogStream::~LogStream()
{
    logger.log(mLogLevel, mOss.str());
}

void initLogger(const std::string &filename)
{
    logger.addAppender(new ConsoleAppender());
    if (!filename.empty())
    {
        FileAppender *appender = new FileAppender();
        appender->tie(filename);
        logger.addAppender(appender);
    }
}