#pragma once

#include <string>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <atomic>
#include <httplib.h>
#include "../core/Config.hpp"

class Auth
{
public:
    enum class RefreshResult
    {
        Success,
        Invalid,
        Failed,
    };

    enum class ReauthState
    {
        Idle,
        InProgress,
        Succeeded,
        Failed,
    };

    Auth(std::string redirectUri = "http://127.0.0.1:8888/callback");
    bool authenticate();
    std::string getAccessToken();
    bool isAuthenticated() const;
    bool ensureFreshToken();
    bool ensureValidSession();
    void applyConfig(const AppConfig &config);

private:
    void saveConfig();
    bool startAuthServer();
    bool waitForAuthCode(std::string &outCode, int timeoutSeconds = 300);
    void stopAuthServer();
    bool exchangeCode(const std::string &code);
    RefreshResult refreshToken();
    void clearRefreshToken();
    bool performReauth(int timeoutSeconds = 300);

    std::string m_clientId;
    std::string m_clientSecret;
    std::string m_redirectUri;
    Config m_config;

    std::string m_accessToken;
    std::string m_refreshToken;
    std::chrono::steady_clock::time_point m_tokenExpiry;

    mutable std::mutex m_tokenMutex;

    std::unique_ptr<httplib::Server> m_authServer;
    std::thread m_authServerThread;
    std::string m_authCode;
    std::mutex m_authCodeMutex;
    std::condition_variable m_authCodeCv;

    std::mutex m_reauthMutex;
    std::condition_variable m_reauthCv;
    ReauthState m_reauthState{ReauthState::Idle};
    std::atomic<bool> m_reauthPending{false};
};
