#pragma once

#include <string_view>
#include <string>

namespace Logger
{
    enum class Level
    {
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };

    void setLogFile(const std::string &logFilePath);
    void cleanupLogFile();
    const char *levelToString(Level level);
    void logMessage(Level level, std::string_view msg);
    inline void debug(std::string_view msg)
    {
        logMessage(Level::Debug, msg);
    }
    inline void info(std::string_view msg)
    {
        logMessage(Level::Info, msg);
    }
    inline void warn(std::string_view msg)
    {
        logMessage(Level::Warn, msg);
    }
    inline void error(std::string_view msg)
    {
        logMessage(Level::Error, msg);
    }
    inline void fatal(std::string_view msg)
    {
        logMessage(Level::Fatal, msg);
    }
}
