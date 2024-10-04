#pragma once

#include <cstdarg> // for va_list, va_start, va_end
#include <cstdio>  // for vsnprintf
#include <ctime>
// #include <iostream>
#include <mutex> // for thread safety
#include <string>
#include <sys/time.h>
#include <cstdio>

enum class LogLevel { CRITICAL, ERROR, WARNING, INFO, DEBUG, TRACE };

class Logger
{
  public:
    static Logger &getLogger(const char *name);
    static void setLevel(const char *level);
    static void setLevel(LogLevel level);
    static void setConsoleSink(bool enable, bool withTimeStamps = true,
                               bool withColors = true);
    static void setRotateFileSink(bool enable, const char *filePath,
                                  size_t maxFileSize = 10 * 1024 * 1024,
                                  int maxNumFiles = 5);

  public:
    void critical(const char *fmt, ...);
    void error(const char *fmt, ...);
    void warning(const char *fmt, ...);
    void warn(const char *fmt, ...);
    void info(const char *fmt, ...);
    void debug(const char *fmt, ...);
    void trace(const char *fmt, ...);
    Logger(const char *name = "");
    virtual ~Logger();

  private:
    static void rotateFileSink();
    static LogLevel logLevel;
    std::string name_;
    std::string nameFormatted_;
    std::mutex logMutex;
    char *msgBuf_{nullptr};
    static constexpr size_t MSG_BUF_SIZE = 8192;

    const char *colorForLevel(LogLevel level) const;
    const char *resetColor() const;
    void log(LogLevel level, const std::string &levelStr, const char *fmt,
             va_list args);
    static bool consoleSinkEnabled_;
    static bool consoleSinkWithTimeStamps_;
    static bool consoleSinkWithColors_;
    static bool rotateFileSinkEnabled_;
    static std::string rotateFileSinkFilePath_;
    static bool rotateFilePathChanged_;
    static size_t rotateFileSinkMaxFileSize_;
    static int rotateFileSinkMaxNumFiles_;
    static FILE *rotateFileSinkFile_;
};
