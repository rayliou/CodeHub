#pragma once

#include <cstdarg> // for va_list, va_start, va_end
#include <cstdio>  // for vsnprintf
#include <ctime>
#include <iostream>
#include <mutex> // for thread safety
#include <string>
#include <sys/time.h>

enum class LogLevel { CRITICAL, ERROR, WARNING, INFO, DEBUG, TRACE };

class Logger
{
  public:
    static Logger &getLogger(const char *name, bool enableColors = true);
    static void setLevel(const char *level);
    static void setLevel(LogLevel level);

  public:
    void critical(const char *fmt, ...);
    void error(const char *fmt, ...);
    void warning(const char *fmt, ...);
    void warn(const char *fmt, ...);
    void info(const char *fmt, ...);
    void debug(const char *fmt, ...);
    void trace(const char *fmt, ...);
    Logger(const char *name = "", bool enableColors = true);

  private:
    static LogLevel logLevel;
    std::string name;
    bool useColors;
    std::mutex logMutex; // Thread safety for logging

    const char *colorForLevel(LogLevel level) const;
    const char *resetColor() const;
    void log(LogLevel level, const std::string &levelStr, const char *fmt,
             va_list args);
};
