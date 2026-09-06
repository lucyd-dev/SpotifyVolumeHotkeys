#include "Logger.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


static std::string logFile = "";
static std::mutex logMutex;
static bool showDialogs = true;
static bool consoleActive = true;

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

    void setShowDialogs(bool show)
    {
        showDialogs = show;
    }

    void setConsoleActive(bool active)
    {
        consoleActive = active;
    }

    void cleanupLogFile()
    {
        std::lock_guard<std::mutex> lock(logMutex);
        if (!logFile.empty())
        {
            std::ofstream ofs(logFile, std::ios::trunc);
        }
    }

    std::vector<wchar_t> utf16(const std::string &utf8)
    {
        std::vector<wchar_t> out;
        std::size_t i = 0;
        const std::size_t n = utf8.size();
        while (i < n)
        {
            unsigned char c = (unsigned char)utf8[i];
            if (c < 0x80)
            {
                out.push_back((wchar_t)c);
                i += 1;
            }
            else if ((c & 0xE0) == 0xC0 && i + 1 < n)
            {
                out.push_back((wchar_t)(((c & 0x1F) << 6) |
                             ((unsigned char)utf8[i + 1] & 0x3F)));
                i += 2;
            }
            else if ((c & 0xF0) == 0xE0 && i + 2 < n)
            {
                out.push_back((wchar_t)(((c & 0x0F) << 12) |
                             (((unsigned char)utf8[i + 1] & 0x3F) << 6) |
                             ((unsigned char)utf8[i + 2] & 0x3F)));
                i += 3;
            }
            else if ((c & 0xF8) == 0xF0 && i + 3 < n)
            {
                unsigned int cp = ((c & 0x07) << 18) |
                                  (((unsigned char)utf8[i + 1] & 0x3F) << 12) |
                                  (((unsigned char)utf8[i + 2] & 0x3F) << 6) |
                                  ((unsigned char)utf8[i + 3] & 0x3F);
                cp -= 0x10000;
                out.push_back((wchar_t)(0xD800 + (cp >> 10)));
                out.push_back((wchar_t)(0xDC00 + (cp & 0x3FF)));
                i += 4;
            }
            else
            {
                out.push_back(0xFFFD);
                i += 1;
            }
        }
        out.push_back(0);
        return out;
    }

    void fatal(std::string_view msg)
    {
        logMessage(Level::Fatal, msg);
        if (!showDialogs)
        {
            return;
        }

        std::ostringstream os;
        os << msg;
        std::vector<wchar_t> text = utf16(os.str());
        MessageBoxW(NULL, text.data(),
                    L"SpotifyVolumeHotkeys \u2014 Fatal Error",
                    MB_OK | MB_ICONERROR);
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

        if (consoleActive)
        {
            std::cout << formatted << std::endl;
        }
        saveToLog(formatted);
    }
}