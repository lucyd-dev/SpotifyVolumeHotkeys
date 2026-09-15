#include "Config.hpp"
#include "Logger.hpp"
#include <nlohmann/json.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>

using json = nlohmann::json;

namespace
{
    int sanitizeInterval(int value, int fallback)
    {
        return value > 0 ? value : fallback;
    }
}

Config::Config()
    : m_path(defaultPath())
{
}

std::filesystem::path Config::defaultPath()
{
    wchar_t *appData = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData) != S_OK || !appData)
    {
        if (appData) CoTaskMemFree(appData);
        throw std::runtime_error("Failed to resolve Roaming AppData folder via SHGetKnownFolderPath");
    }
    std::filesystem::path dir = std::filesystem::path(appData) / "SpotifyVolumeHotkeys/config";
    CoTaskMemFree(appData);
    return dir / "config.json";
}

AppConfig Config::load()
{
    AppConfig config;

    auto parent = m_path.parent_path();
    if (!parent.empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec)
        {
            Logger::error("Failed to create config directory: " + parent.string() +
                          " (" + ec.message() + ")");
        }
    }

    if (!std::filesystem::exists(m_path))
    {
        Logger::info("Config file not found at " + m_path.string() + ". A new template will be created.");
        if (save(config))
        {
            Logger::debug("Created default config file at " + m_path.string());
        }
        return config;
    }

    std::ifstream file(m_path);
    if (!file.is_open())
    {
        Logger::error("Failed to open config file for reading: " + m_path.string());
        m_lastLoadOk = false;
        return config;
    }

    try
    {
        json data;
        file >> data;
        m_lastLoadOk = true;

        if (data.contains("client_id") && data["client_id"].is_string())
        {
            config.clientId = data["client_id"].get<std::string>();
        }
        if (data.contains("client_secret") && data["client_secret"].is_string())
        {
            config.clientSecret = data["client_secret"].get<std::string>();
        }
        if (data.contains("refresh_token") && data["refresh_token"].is_string())
        {
            config.refreshToken = data["refresh_token"].get<std::string>();
        }
        if (data.contains("autostart") && data["autostart"].is_boolean())
        {
            config.autostart = data["autostart"].get<bool>();
        }
        if (data.contains("hotkeys") && data["hotkeys"].is_object())
        {
            const auto &hotkeys = data["hotkeys"];
            if (hotkeys.contains("volume_down") && hotkeys["volume_down"].is_string())
            {
                config.volumeDownKey = hotkeys["volume_down"].get<std::string>();
            }
            if (hotkeys.contains("volume_up") && hotkeys["volume_up"].is_string())
            {
                config.volumeUpKey = hotkeys["volume_up"].get<std::string>();
            }
        }
        if (data.contains("pollingIntervals") && data["pollingIntervals"].is_object())
        {
            const auto &intervals = data["pollingIntervals"];
            if (intervals.contains("input") && intervals["input"].is_number_integer())
            {
                config.inputTimerInterval =
                    sanitizeInterval(intervals["input"].get<int>(), config.inputTimerInterval);
            }
            if (intervals.contains("player") && intervals["player"].is_number_integer())
            {
                config.playerTimerInterval =
                    sanitizeInterval(intervals["player"].get<int>(), config.playerTimerInterval);
            }
        }

        Logger::debug("Successfully loaded config from " + m_path.string());
    }
    catch (const std::exception &e)
    {
        Logger::error(std::string("Failed to parse config JSON: ") + e.what());
        m_lastLoadOk = false;
    }

    return config;
}

bool Config::save(const AppConfig &config) const
{
    json data = json::object();
    if (std::filesystem::exists(m_path))
    {
        std::ifstream inFile(m_path);
        if (inFile.is_open())
        {
            try { inFile >> data; } catch (...) { data = json::object(); }
        }
    }

    data["client_id"] = config.clientId;
    data["client_secret"] = config.clientSecret;
    data["refresh_token"] = config.refreshToken;
    data["autostart"] = config.autostart;
    data["hotkeys"]["volume_down"] = config.volumeDownKey;
    data["hotkeys"]["volume_up"] = config.volumeUpKey;
    data["pollingIntervals"]["input"] = config.inputTimerInterval;
    data["pollingIntervals"]["player"] = config.playerTimerInterval;

    auto parent = m_path.parent_path();
    if (!parent.empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    std::ofstream outFile(m_path);
    if (!outFile.is_open())
    {
        Logger::error("Failed to open config file for writing: " + m_path.string());
        return false;
    }

    outFile << std::setw(4) << data << std::endl;
    Logger::debug("Config successfully saved to " + m_path.string());
    return true;
}

std::filesystem::file_time_type Config::lastWriteTime() const
{
    try
    {
        return std::filesystem::last_write_time(m_path);
    }
    catch (...)
    {
        return std::filesystem::file_time_type{};
    }
}
