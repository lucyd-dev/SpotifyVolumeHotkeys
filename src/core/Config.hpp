#pragma once

#include <filesystem>
#include <string>

struct AppConfig
{
    std::string clientId;
    std::string clientSecret;
    std::string refreshToken;
};

class Config
{
public:
    Config();

    AppConfig load();
    bool save(const AppConfig &config) const;
    const std::filesystem::path &path() const noexcept { return m_path; }

private:
    static std::filesystem::path defaultPath();

    std::filesystem::path m_path;
};