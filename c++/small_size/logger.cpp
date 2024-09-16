#include "logger.h"
#include <map>
#include <memory>

Logger &Logger::getLogger(const char *name, bool enableColors)
{
    std::string nameStr = (name && name[0] != '\0') ? std::string(name) : "";
    if (!nameStr.empty()) {
        size_t lastSlash = nameStr.find_last_of("/");
        if (lastSlash != std::string::npos) {
            nameStr = nameStr.substr(lastSlash + 1);
        }
    }
    name = nameStr.c_str();

    static std::map<std::string, std::unique_ptr<Logger>> loggerMap;
    auto it = loggerMap.find(name);
    if (it == loggerMap.end()) {
        loggerMap[name] = std::make_unique<Logger>(name, enableColors);
    }
    return *loggerMap[name];
}

void Logger::setLevel(const char *level)
{
    std::string levelStr(level);
    for (auto &c : levelStr) {
        c = toupper(c);
    }
    if (levelStr == "CRITICAL") {
        logLevel = LogLevel::CRITICAL;
    }
    else if (levelStr == "ERROR") {
        logLevel = LogLevel::ERROR;
    }
    else if (levelStr == "WARNING" || levelStr == "WARN") {
        logLevel = LogLevel::WARNING;
    }
    else if (levelStr == "INFO") {
        logLevel = LogLevel::INFO;
    }
    else if (levelStr == "DEBUG") {
        logLevel = LogLevel::DEBUG;
    }
    else if (levelStr == "TRACE") {
        logLevel = LogLevel::TRACE;
    }
}

void Logger::setLevel(LogLevel level) { logLevel = level; }
Logger::Logger(const char *name, bool enableColors)
    : name(name), useColors(enableColors)
{
}

void Logger::critical(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log(LogLevel::CRITICAL, "CRITICAL", fmt, args);
    va_end(args);
}

void Logger::error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log(LogLevel::ERROR, "ERROR", fmt, args);
    va_end(args);
}

void Logger::warning(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log(LogLevel::WARNING, "WARNING", fmt, args);
    va_end(args);
}

void Logger::warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log(LogLevel::WARNING, "WARNING", fmt, args);
    va_end(args);
}

void Logger::info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log(LogLevel::INFO, "INFO", fmt, args);
    va_end(args);
}

void Logger::debug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log(LogLevel::DEBUG, "DEBUG", fmt, args);
    va_end(args);
}
void Logger::trace(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log(LogLevel::TRACE, "TRACE", fmt, args);
    va_end(args);
}

const char *Logger::colorForLevel(LogLevel level) const
{
    if (!useColors)
        return "";
    switch (level) {
    case LogLevel::CRITICAL:
        return "\033[1;31m"; // Bright Red
    case LogLevel::ERROR:
        return "\033[31m"; // Red
    case LogLevel::WARNING:
        return "\033[33m"; // Yellow
    case LogLevel::INFO:
        return "\033[32m"; // Green
    case LogLevel::DEBUG:
        return "\033[34m"; // Blue
    case LogLevel::TRACE:
        return "\033[90m"; // Light Grey (or Bright Black)
    default:
        return "";
    }
}
const char *Logger::resetColor() const
{
    return "\033[0m"; // Reset color
}
void Logger::log(LogLevel level, const std::string &levelStr, const char *fmt,
                 va_list args)
{
    if (level > logLevel)
        return;
    std::lock_guard<std::mutex> lock(logMutex);
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm *tm_info = localtime(&tv.tv_sec);
    char timeBuf[30];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", tm_info);
    snprintf(timeBuf + 19, sizeof(timeBuf) - 19, ".%03ld", tv.tv_usec / 1000);
    char msgBuf[2048];
    std::vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);

    std::cout << "[" << timeBuf << "] "
              << (name.empty() ? "" : "[" + name + "] ") << colorForLevel(level)
              << levelStr << resetColor() << ": " << msgBuf << std::endl;
}
