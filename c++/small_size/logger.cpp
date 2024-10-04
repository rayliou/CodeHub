#include "logger.h"
#include <map>
#include <memory>
#include <stdexcept>
#include <unistd.h>

Logger &Logger::getLogger(const char *name)
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
        loggerMap[name] = std::make_unique<Logger>(name);
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
Logger::Logger(const char *name)
    : name_(name), msgBuf_(new char[MSG_BUF_SIZE])
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

    // Lock the mutex to prevent concurrent log writes
    std::lock_guard<std::mutex> lock(logMutex);

    // Get the current time
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm tm_info;
    localtime_r(&tv.tv_sec, &tm_info);

    // Format the timestamp
    char timeBuf[30];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm_info);
    snprintf(timeBuf + 19, sizeof(timeBuf) - 19, ".%03d", static_cast<int>(tv.tv_usec / 1000));

    // Format the log message
    std::vsnprintf(msgBuf_, MSG_BUF_SIZE, fmt, args);

    // Create formatted strings for timestamp, logger name, and log level
    std::string timeFormatted = std::string("[") + timeBuf + std::string("] ");
    std::string nameFormatted = name_.empty() ? "" : std::string("[") + name_ + std::string("] ");
    std::string levelFormatted = levelStr;
    levelFormatted.resize(10, ' ');
    std::string coloredLevel = colorForLevel(level) + levelFormatted + resetColor();

    // Log to console if enabled
    if (consoleSinkEnabled_) {
        printf("%s%s%s%s\n",
            consoleSinkWithTimeStamps_ ? timeFormatted.c_str() : "",
            consoleSinkWithColors_ ? coloredLevel.c_str() : levelFormatted.c_str(),
            nameFormatted.c_str(),
            msgBuf_
        );
    }

    // Log to file if enabled
    if (rotateFileSinkEnabled_ && rotateFileSinkFile_) {
        fprintf(rotateFileSinkFile_, "%s%s%s%s\n",
            timeFormatted.c_str(), levelFormatted.c_str(), nameFormatted.c_str(), msgBuf_
        );
        rotateFileSink();
    }
}



Logger::~Logger() { delete[] msgBuf_; }

bool Logger::consoleSinkEnabled_{true};
bool Logger::consoleSinkWithTimeStamps_{true};
bool Logger::consoleSinkWithColors_{true};

bool Logger::rotateFileSinkEnabled_{false};
std::string Logger::rotateFileSinkFilePath_{};
bool Logger::rotateFilePathChanged_{false};
size_t Logger::rotateFileSinkMaxFileSize_{10 * 1024 * 1024};
int Logger::rotateFileSinkMaxNumFiles_{5};
FILE *Logger::rotateFileSinkFile_{nullptr};

void Logger::setConsoleSink(bool enable, bool withTimeStamps, bool withColors)
{
    consoleSinkEnabled_ = enable;
    consoleSinkWithTimeStamps_ = withTimeStamps;
    consoleSinkWithColors_ = withColors;
}
void Logger::setRotateFileSink(bool enable, const char *filePath,
                                  size_t maxFileSize, int maxNumFiles)
{
    rotateFileSinkEnabled_ = enable;
    rotateFilePathChanged_ = rotateFileSinkFilePath_ != filePath;
    rotateFileSinkFilePath_ = filePath;
    rotateFileSinkMaxFileSize_ = maxFileSize;
    rotateFileSinkMaxNumFiles_ = maxNumFiles;
    rotateFileSink();
    rotateFilePathChanged_ = false;
}
void Logger::rotateFileSink()
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
            fprintf(stderr, "Failed to get file size for %s\n", rotateFileSinkFilePath_.c_str());
            throw std::runtime_error("Failed to get file size for " + rotateFileSinkFilePath_);
        }

        if (currentSize > rotateFileSinkMaxFileSize_) {
            fclose(rotateFileSinkFile_);
            rotateFileSinkFile_ = nullptr;

            // Implement file rotation based on max files policy
            for (int i = rotateFileSinkMaxNumFiles_ - 1; i > 0; --i) {
                std::string oldName = rotateFileSinkFilePath_ + "." + std::to_string(i);
                std::string newName = rotateFileSinkFilePath_ + "." + std::to_string(i + 1);
                if (i == rotateFileSinkMaxNumFiles_ - 1) {
                    if (access(oldName.c_str(), F_OK) == 0) {
                        if (remove(oldName.c_str()) != 0) {
                            fprintf(stderr, "Failed to remove old file: %s\n", oldName.c_str());
                        }
                    }
                } else {
                    if (access(oldName.c_str(), F_OK) == 0) {
                        if (rename(oldName.c_str(), newName.c_str()) != 0) {
                            fprintf(stderr, "Failed to rename file: %s to %s\n", oldName.c_str(), newName.c_str());
                        }
                    }
                }
            }

            if (access(rotateFileSinkFilePath_.c_str(), F_OK) == 0) {
                if (rename(rotateFileSinkFilePath_.c_str(), (rotateFileSinkFilePath_ + ".1").c_str()) != 0) {
                    fprintf(stderr, "Failed to rename file: %s\n", rotateFileSinkFilePath_.c_str());
                }
            }
        }
    }

    // Open a new file if needed
    if (!rotateFileSinkFile_) {
        rotateFileSinkFile_ = fopen(rotateFileSinkFilePath_.c_str(), "a");
        if (!rotateFileSinkFile_) {
            fprintf(stderr, "Failed to open rotate file sink at %s\n", rotateFileSinkFilePath_.c_str());
            throw std::runtime_error("Failed to open rotate file sink at " + rotateFileSinkFilePath_);
        }
    }
}

LogLevel Logger::logLevel{LogLevel::INFO};
int main() {
    //Default logger    
    Logger &logger = Logger::getLogger("test");
    logger.setLevel("INFO");
    logger.info("This is an info message");
    logger.critical("This is a critical message");

    //Rotate logger
    Logger &rotateLogger = Logger::getLogger("rotate");
    rotateLogger.setRotateFileSink(true, "rotate.log", 10 * 1024 * 1024, 5);
    rotateLogger.info("This is an info message");
    rotateLogger.critical("This is a critical message");
    return 0;
}

