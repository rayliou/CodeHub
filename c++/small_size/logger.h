#pragma once

#include <cstdarg> // for va_list, va_start, va_end
#include <cstdio>  // for vsnprintf
#include <cstdio>
#include <ctime>
#include <mutex> // for thread safety
#include <string>
#include <sys/time.h>
#include <vector>

enum class LogLevel { CRITICAL, ERROR, WARNING, INFO, DEBUG, TRACE };

class LoggerOutput
{
  public:
    static bool isLoggerMessage(const char* msg);
    static LoggerOutput *instance();
    void log(const std::string &loggerName, LogLevel level,
             const std::string &levelStr, const char *fmt, va_list args);
    void setLevel(const char *level);
    void setLevel(LogLevel level);
    void setConsoleSink(bool enable, bool withTimeStamps = true,
                        bool withColors = true);
    void setRotateFileSink(bool enable, const char *filePath,
                           size_t maxFileSize = 10 * 1024 * 1024,
                           int maxNumFiles = 5);
    void setFlushOnLog(bool enable) { flushOnLog_ = enable; }
    LoggerOutput();
    virtual ~LoggerOutput() {}
    void flush();

  private:
    static constexpr size_t MSG_BUF_SIZE = 8192;
    void rotateFileSink();
    LogLevel logLevel;
    std::vector<char> msgBufferChars_;
    char *msgBuf_{nullptr};

    const char *colorForLevel(LogLevel level) const;
    const char *resetColor() const;
    std::mutex logMutex;
    bool flushOnLog_{false};
    bool consoleSinkEnabled_{true};
    bool consoleSinkWithTimeStamps_{true};
    bool consoleSinkWithColors_{true};
    bool rotateFileSinkEnabled_{false};
    std::string rotateFileSinkFilePath_{"logs/log.txt"};
    bool rotateFilePathChanged_{false};
    size_t rotateFileSinkMaxFileSize_{10 * 1024 * 1024};
    int rotateFileSinkMaxNumFiles_{5};
    FILE *rotateFileSinkFile_{nullptr};
};

class Logger
{
  public:
    Logger(const std::string &name = "");

  public:
    void critical(const char *fmt, ...);
    void error(const char *fmt, ...);
    void warning(const char *fmt, ...);
    void warn(const char *fmt, ...);
    void info(const char *fmt, ...);
    void debug(const char *fmt, ...);
    void trace(const char *fmt, ...);
    void flush() { loggerOutput_->flush(); }
  private:
    std::string name_;
    std::string nameFormatted_;
    LoggerOutput *loggerOutput_{nullptr};
};
