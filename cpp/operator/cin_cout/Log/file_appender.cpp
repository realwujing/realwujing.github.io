#include "file_appender.h"

void FileAppender::append(const std::string &line)
{
    if (mLogFile.is_open())
    {
        mLogFile.write(line.c_str(), line.length());
        mLogFile.flush();
    }
}

void FileAppender::tie(const std::string &filename)
{
    if (mLogFile.is_open())
        mLogFile.close();
    mLogFile.open(filename.c_str(), std::ios::app | std::ios::out);
}