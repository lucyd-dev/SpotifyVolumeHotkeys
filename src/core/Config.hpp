#pragma once

#include <filesystem>
#include <string>

struct AppConfig
{
    std::string clientId;
    std::string clientSecret;
    std::string refreshToken;
    std::string volumeDownKey = "F13";
    std::string volumeUpKey = "F14";
    bool autostart = true;
};

class Config
{
public:
    Config();

    AppConfig load();
    bool save(const AppConfig &config) const;
    bool lastLoadSuccess() const noexcept { return m_lastLoadOk; }
    const std::filesystem::path &path() const noexcept { return m_path; }
    std::filesystem::file_time_type lastWriteTime() const;

private:
    static std::filesystem::path defaultPath();

    std::filesystem::path m_path;
    bool m_lastLoadOk = true;
};