#include "logger.h"
#include <map>
#include <memory>
#include <stdexcept>
#include <unistd.h>
#include <cstring>

LoggerOutput *LoggerOutput::instance()
{
    static LoggerOutput instance;
    return &instance;
}

void LoggerOutput::setLevel(const char *level)
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

void LoggerOutput::setLevel(LogLevel level) { logLevel = level; }
LoggerOutput::LoggerOutput()
{
    msgBufferChars_.reserve(MSG_BUF_SIZE);
    msgBuf_ = msgBufferChars_.data();
}

const char *LoggerOutput::colorForLevel(LogLevel level) const
{
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
const char *LoggerOutput::resetColor() const
{
    return "\033[0m"; // Reset color
}
void LoggerOutput::log(const std::string &loggerName, LogLevel level,
                       const std::string &levelStr, const char *fmt,
                       va_list args)
{
    if (level > logLevel)
        return;
    std::lock_guard<std::mutex> lock(logMutex);

    // Get the current time
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm tm_info;
    localtime_r(&tv.tv_sec, &tm_info);

    // Format the timestamp
    char timeBuf[30];
    size_t len =
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm_info);
    snprintf(timeBuf + len, sizeof(timeBuf) - len, ".%03d",
             static_cast<int>(tv.tv_usec / 1000));
    // Format the log message
    std::vsnprintf(msgBuf_, MSG_BUF_SIZE, fmt, args);

    // Create formatted strings for timestamp, logger name, and log level
    std::string timeFormatted = std::string("[") + timeBuf + std::string("] ");
    std::string levelFormatted = levelStr;
    levelFormatted.resize(10, ' ');
    std::string coloredLevel =
        colorForLevel(level) + levelFormatted + resetColor();
    // Add brackets around level
    coloredLevel = "[" + coloredLevel + "]";
    levelFormatted = "[" + levelFormatted + "]";

    // Log to console if enabled
    if (consoleSinkEnabled_) {
        printf("%s%s%s%s\n",
               consoleSinkWithTimeStamps_ ? timeFormatted.c_str() : "",
               consoleSinkWithColors_ ? coloredLevel.c_str()
                                      : levelFormatted.c_str(),
               loggerName.c_str(), msgBuf_);
        if (flushOnLog_) {
            fflush(stdout);
        }
    }

    // Log to file if enabled
    if (rotateFileSinkEnabled_ && rotateFileSinkFile_) {
        fprintf(rotateFileSinkFile_, "%s%s%s%s\n", timeFormatted.c_str(),
                levelFormatted.c_str(), loggerName.c_str(), msgBuf_);
        rotateFileSink();
        if (flushOnLog_) {
            fflush(rotateFileSinkFile_);
        }
    }
}

void LoggerOutput::setConsoleSink(bool enable, bool withTimeStamps,
                                  bool withColors)
{
    std::lock_guard<std::mutex> lock(logMutex);
    consoleSinkEnabled_ = enable;
    consoleSinkWithTimeStamps_ = withTimeStamps;
    consoleSinkWithColors_ = withColors;
}
void LoggerOutput::setRotateFileSink(bool enable, const char *filePath,
                                     size_t maxFileSize, int maxNumFiles)
{
    std::lock_guard<std::mutex> lock(logMutex);
    rotateFileSinkEnabled_ = enable;
    rotateFilePathChanged_ = rotateFileSinkFilePath_ != filePath;
    rotateFileSinkFilePath_ = filePath;
    rotateFileSinkMaxFileSize_ = maxFileSize;
    rotateFileSinkMaxNumFiles_ = maxNumFiles;
    rotateFileSink();
    rotateFilePathChanged_ = false;
}
void LoggerOutput::rotateFileSink()
{
    // File path changed, close the old file and open a new one
    if (rotateFilePathChanged_ && rotateFileSinkFile_) {
        fclose(rotateFileSinkFile_);
        rotateFileSinkFile_ = nullptr;
    }

    // Close and rotate file if it exceeds the maximum size
    if (rotateFileSinkFile_ && rotateFileSinkMaxFileSize_ > 0) {
        long currentSize = ftell(rotateFileSinkFile_);
        if (currentSize == -1L) {
            fprintf(stderr, "Failed to get file size for %s\n",
                    rotateFileSinkFilePath_.c_str());
            throw std::runtime_error("Failed to get file size for " +
                                     rotateFileSinkFilePath_);
        }

        if (currentSize > rotateFileSinkMaxFileSize_) {
            fclose(rotateFileSinkFile_);
            rotateFileSinkFile_ = nullptr;

            std::string oldestFile =
                rotateFileSinkFilePath_ + "." +
                std::to_string(rotateFileSinkMaxNumFiles_ - 1);
            if (remove(oldestFile.c_str()) != 0 && errno != ENOENT) {
                fprintf(stderr, "Failed to remove old file: %s\n",
                        oldestFile.c_str());
            }
            for (int i = rotateFileSinkMaxNumFiles_ - 1; i >= 1; --i) {
                std::string oldName =
                    rotateFileSinkFilePath_ + "." + std::to_string(i - 1);
                std::string newName =
                    rotateFileSinkFilePath_ + "." + std::to_string(i);
                if (rename(oldName.c_str(), newName.c_str()) != 0 &&
                    errno != ENOENT) {
                    fprintf(stderr, "Failed to rename file: %s to %s\n",
                            oldName.c_str(), newName.c_str());
                }
            }
            if (rename(rotateFileSinkFilePath_.c_str(),
                       (rotateFileSinkFilePath_ + ".0").c_str()) != 0 &&
                errno != ENOENT) {
                fprintf(stderr, "Failed to rename file: %s\n",
                        rotateFileSinkFilePath_.c_str());
            }
        }
    }

    // Open a new file if needed
    if (!rotateFileSinkFile_) {
        rotateFileSinkFile_ = fopen(rotateFileSinkFilePath_.c_str(), "a");
        if (!rotateFileSinkFile_) {
            fprintf(stderr, "Failed to open rotate file sink at %s\n",
                    rotateFileSinkFilePath_.c_str());
            throw std::runtime_error("Failed to open rotate file sink at " +
                                     rotateFileSinkFilePath_);
        }
    }
}
void LoggerOutput::flush()
{
    if (rotateFileSinkFile_) {
        fflush(rotateFileSinkFile_);
    }
}   
bool LoggerOutput::isLoggerMessage(const char* msg)
{
    return strncmp(msg, "[", 1) == 0;
}
Logger::Logger(const std::string &name)
    : name_(name), loggerOutput_(LoggerOutput::instance())
{
    if (!name_.empty()) {
        size_t lastSlash = name_.find_last_of("/");
        if (lastSlash != std::string::npos) {
            name_ = name_.substr(lastSlash + 1);
        }
    }
    nameFormatted_ =
        name_.empty() ? "" : std::string("[") + name_ + std::string("] ");
}
void Logger::critical(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    loggerOutput_->log(nameFormatted_, LogLevel::CRITICAL, "CRITICAL", fmt,
                       args);
    va_end(args);
}

void Logger::error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    loggerOutput_->log(nameFormatted_, LogLevel::ERROR, "ERROR", fmt, args);
    va_end(args);
}

void Logger::warning(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    loggerOutput_->log(nameFormatted_, LogLevel::WARNING, "WARNING", fmt, args);
    va_end(args);
}

void Logger::warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    loggerOutput_->log(nameFormatted_, LogLevel::WARNING, "WARNING", fmt, args);
    va_end(args);
}

void Logger::info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    loggerOutput_->log(nameFormatted_, LogLevel::INFO, "INFO", fmt, args);
    va_end(args);
}

void Logger::debug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    loggerOutput_->log(nameFormatted_, LogLevel::DEBUG, "DEBUG", fmt, args);
    va_end(args);
}
void Logger::trace(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    loggerOutput_->log(nameFormatted_, LogLevel::TRACE, "TRACE", fmt, args);
    va_end(args);
}
#if 0

int main()
{
    // Rotate logger
    Logger rotateLogger("rotate");
    LoggerOutput::instance()->setRotateFileSink(true, "rotate.log", 1024, 5);

    Logger log_1("console without formatting");
    LoggerOutput::instance()->setLevel("DEBUG");
    LoggerOutput::instance()->setConsoleSink(true, false, false);
    log_1.info("This is an info message");
    log_1.critical("This is a critical message");

    Logger log_2("console with colors");
    LoggerOutput::instance()->setLevel("TRACE");
    LoggerOutput::instance()->setConsoleSink(true, false, true);
    log_2.warn("Only with colors");
    log_2.info("This is an info message");
    log_2.critical("This is a critical message  ");
    log_2.trace("This is a trace message  ");
    log_2.warn("Close console logger");

    LoggerOutput::instance()->setConsoleSink(false);
    rotateLogger.info("This is an info message");
    rotateLogger.critical("This is a critical message");

    Logger log_console_all("console with formatting");
    LoggerOutput::instance()->setLevel("TRACE");
    LoggerOutput::instance()->setConsoleSink(true, true, true);
    log_console_all.warn("Open console logger %s", "with formatting");
    log_console_all.info("This is an info message %d", 1);
    log_console_all.critical("This is a critical message  %d", 2);
    log_console_all.trace("This is a trace message  %d", 3);

    // Test rotate logger
    rotateLogger.info("This is an info message");
    rotateLogger.critical("This is a critical message");
    return 0;
}
#endif