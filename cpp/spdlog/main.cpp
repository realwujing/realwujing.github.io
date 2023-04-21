// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <iostream>
#include <memory>

// spd 带行号的打印，同时输出console和文件
#define DEBUG(...)                                                  \
    SPDLOG_LOGGER_DEBUG(spdlog::default_logger_raw(), __VA_ARGS__); \
    SPDLOG_LOGGER_DEBUG(spdlog::get("daily_logger"), __VA_ARGS__)
#define LOG(...)                                                   \
    SPDLOG_LOGGER_INFO(spdlog::default_logger_raw(), __VA_ARGS__); \
    SPDLOG_LOGGER_INFO(spdlog::get("daily_logger"), __VA_ARGS__)
#define WARN(...)                                                  \
    SPDLOG_LOGGER_WARN(spdlog::default_logger_raw(), __VA_ARGS__); \
    SPDLOG_LOGGER_WARN(spdlog::get("daily_logger"), __VA_ARGS__)
#define ERROR(...)                                                  \
    SPDLOG_LOGGER_ERROR(spdlog::default_logger_raw(), __VA_ARGS__); \
    SPDLOG_LOGGER_ERROR(spdlog::get("daily_logger"), __VA_ARGS__)

int main(int argc, char *argv[])
{
    // 按文件大小
    //auto file_logger = spdlog::rotating_logger_mt("file_log", "log/log.log", 1024 * 1024 * 100, 3);
    // 每天2:30 am 新建一个日志文件
    auto logger = spdlog::daily_logger_mt("daily_logger", "logs/daily.txt", 2, 30);
    // 遇到warn flush日志，防止丢失
    logger->flush_on(spdlog::level::warn);
    //每三秒刷新一次
    spdlog::flush_every(std::chrono::seconds(3));

    // Set the default logger to file logger
    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::debug); // Set global log level to debug

    // change log pattern
    // %s：文件名
    // %#：行号
    // %!：函数名
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S [%l] [%t] - <%s>|<%#>|<%!>,%v");

    LOG("test info");
    ERROR("test error");

    // Release and close all loggers
    spdlog::drop_all();
}
