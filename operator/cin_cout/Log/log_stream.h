#include <string>
#include <sstream>

class LogStream
{
public:
    LogStream(const std::string &level);
    ~LogStream();
    template<typename T>
    LogStream &operator<<(T log)
    {
        mOss << log;
        return *this;
    }
private: 
    std::string mLogLevel;
    std::ostringstream mOss;
};

void initLogger(const std::string &filename = "");

#define LOG(level) LogUtil(#level)
#define INFO LOG(INFO)
#define WARN LOG(WARN)
#define ERROR LOG(ERROR)
#define DEBUG LOG(DEBUG)