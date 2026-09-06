#pragma once

#include <string_view>
#include <string>
#include <vector>

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
    void setShowDialogs(bool show);
    void setConsoleActive(bool active);
    const char *levelToString(Level level);
    void logMessage(Level level, std::string_view msg);
    std::vector<wchar_t> utf16(const std::string &utf8);

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
    void fatal(std::string_view msg);
}