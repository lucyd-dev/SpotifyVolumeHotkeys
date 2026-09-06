#include "Logger.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>


static std::string logFile = "";
static std::mutex logMutex;

static void saveToLog(const std::string &entry) {
    if (!logFile.empty())
    {
        std::ofstream ofs(logFile, std::ios::app);
        if (ofs.is_open())
        {
            ofs << entry << "\n";
        }
    }
}

namespace Logger
{
    const char* levelToString(Level level)
    {
        switch (level)
        {
            case Level::Debug: return "DEBUG";
            case Level::Info:  return "INFO ";
            case Level::Warn:  return "WARN ";
            case Level::Error: return "ERROR";
            case Level::Fatal: return "FATAL";
            default:           return "???  ";
        }
    }

    void setLogFile(const std::string &logFilePath)
    {
        logFile = logFilePath;
    }

    void cleanupLogFile()
    {
        std::lock_guard<std::mutex> lock(logMutex);
        if (!logFile.empty())
        {
            std::ofstream ofs(logFile, std::ios::trunc);
        }
    }

    void logMessage(Level level, std::string_view msg)
    {
        std::lock_guard<std::mutex> lock(logMutex);

        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &in_time_t);
#else
        localtime_r(&in_time_t, &tm_buf);
#endif
        std::stringstream ss;
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");

        std::stringstream entry;
        entry << "[" << ss.str() << "] "
              << "[" << levelToString(level) << "] "
              << msg;

        std::string formatted = entry.str();

        std::cout << formatted << std::endl;
        saveToLog(formatted);
    }
}
