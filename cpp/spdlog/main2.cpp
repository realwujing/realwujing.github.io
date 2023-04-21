
#ifndef LOGGER_INCLUDE
#define LOGGER_INCLUDE

#include <iostream>
#include <sstream>
#include <memory>

// 强枚举类型，阻止普通变量代入
enum class LOGGER_LEVEL
{
    LOGGER_INFO,
    LOGGER_DEBUG,
    LOGGER_ERROR,
    LOGGER_EXCEPTION,
    LOGGER_FATAL
};

class Logger
{
public:
    // C++默认构造
    Logger() = default;

    // 日志流使用完自动释放，析构中完成流的输出
    virtual ~Logger();

    // 流启动器，可接收后续输入
    std::ostream &Start(LOGGER_LEVEL logLevel);

private:
    // 此接口是为了以后的拓展，比如控制台流：std::cout ,字符串流：std::ostringstream , 文件流：std::ofstream
    std::ostream &Stream();

    // 析构时写日志
    void WriteLog(LOGGER_LEVEL logLevel, const std::string &logMsg);

    // 暂使用字符串流
    std::ostringstream m_logStream;

    // 通常来说，日志级别默认是info级别
    LOGGER_LEVEL m_logLevel = LOGGER_LEVEL::LOGGER_INFO;
};

// 智能指针，自动构造与析构
inline auto GetLogger() -> std::shared_ptr<Logger>
{
    return std::make_shared<Logger>();
}

#define LOGGER GetLogger()->start(LOGGER_LEVEL::INFO)
#define LOGGER_INFO GetLogger()->start(LOGGER_LEVEL::LOGGER_INFO)
#define LOGGER_DEBUG GetLogger()->start(LOGGER_LEVEL::LOGGER_DEBUG)
#define LOGGER_ERROR GetLogger()->start(LOGGER_LEVEL::LOGGER_ERROR)
#define LOGGER_EXCEPTION GetLogger()->start(LOGGER_LEVEL::LOGGER_EXCEPTION)
#define LOGGER_FATAL GetLogger()->start(LOGGER_LEVEL::LOGGER_FATAL)

#endif