#include <string>
#include <fstream>
#include "appender.h"

class FileAppender : public Appender
{
public:
    virtual ~FileAppender() {}
    virtual void append(const std::string &line);
    void tie(const std::string &);

private:
    std::fstream mLogFile;
};