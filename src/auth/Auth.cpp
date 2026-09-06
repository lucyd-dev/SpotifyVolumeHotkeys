#include "Auth.hpp"
#include "../core/HttpClient.hpp"
#include "../core/Logger.hpp"
#include <nlohmann/json.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <thread>
#include <chrono>

using json = nlohmann::json;

Auth::Auth(std::string redirectUri)
    : m_redirectUri(std::move(redirectUri)),
      m_config()
{
}

bool Auth::authenticate()
{
    AppConfig appConfig = m_config.load();
    m_clientId = std::move(appConfig.clientId);
    m_clientSecret = std::move(appConfig.clientSecret);
    {
        std::lock_guard<std::mutex> lock(m_tokenMutex);
        m_refreshToken = std::move(appConfig.refreshToken);
    }

    if (m_clientId.empty() || m_clientSecret.empty())
    {
        Logger::error("Missing client_id or client_secret. Please provide them in config.json.");
        return false;
    }

    bool hasRefreshToken = false;
    {
        std::lock_guard<std::mutex> lock(m_tokenMutex);
        hasRefreshToken = !m_refreshToken.empty();
    }

    if (hasRefreshToken)
    {
        Logger::debug("Found saved refresh token. Attempting authentication...");
        auto result = refreshToken();
        if (result == RefreshResult::Success)
        {
            Logger::debug("Authenticated using saved refresh token.");
            return true;
        }
        if (result == RefreshResult::Invalid)
        {
            Logger::warn("Saved refresh token was invalid or expired. Falling back to browser login.");
        }
        else
        {
            Logger::warn("Saved refresh token refresh failed (transient). Falling back to browser login.");
        }
    }

    return performReauth(300);
}

bool Auth::performReauth(int timeoutSeconds)
{
    if (!startAuthServer())
    {
        Logger::error("Failed to start auth server.");
        return false;
    }

    std::string authUrl = "https://accounts.spotify.com/authorize?client_id=" + m_clientId +
                          "&response_type=code&redirect_uri=" + m_redirectUri +
                          "&scope=user-modify-playback-state%20user-read-playback-state" +
                          "&show_dialog=true";
    Logger::info("Opening browser for Spotify login...");
    ShellExecuteA(NULL, "open", authUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);

    std::string authCode;
    if (!waitForAuthCode(authCode, timeoutSeconds))
    {
        Logger::error("Failed to retrieve authorization code (timed out or no callback).");
        return false;
    }

    if (exchangeCode(authCode))
    {
        saveConfig();
        return true;
    }

    return false;
}

bool Auth::startAuthServer()
{
    stopAuthServer();

    m_authServer = std::make_unique<httplib::Server>();

    m_authServer->Get("/callback", [this](const auto &req, auto &res)
    {
        if (req.has_param("code"))
        {
            {
                std::lock_guard<std::mutex> lock(m_authCodeMutex);
                m_authCode = req.get_param_value("code");
            }
            m_authCodeCv.notify_all();
            res.set_content("<html><body><h1>Authentication successful!</h1><p>You can close this window now.</p></body></html>", "text/html");
        }
        else if (req.has_param("error"))
        {
            auto error = req.get_param_value("error");
            Logger::error("Authentication error: " + error);
            res.set_content("Authentication failed. Error: " + error, "text/plain");
        }
        else
        {
            res.set_content("Authentication failed. No code found.", "text/plain");
        }

        m_authServer->stop();
    });

    m_authServerThread = std::thread([this]()
    {
        m_authServer->listen("127.0.0.1", 8888);
    });

    return true;
}

bool Auth::waitForAuthCode(std::string &outCode, int timeoutSeconds)
{
    std::unique_lock<std::mutex> lock(m_authCodeMutex);
    bool got = m_authCodeCv.wait_for(lock, std::chrono::seconds(timeoutSeconds),
                                     [this]() { return !m_authCode.empty(); });
    if (got)
    {
        outCode = m_authCode;
        m_authCode.clear();
    }
    if (m_authServerThread.joinable())
    {
        m_authServerThread.join();
    }
    m_authServer.reset();
    return got;
}

void Auth::stopAuthServer()
{
    if (m_authServer)
    {
        m_authServer->stop();
    }
    if (m_authServerThread.joinable())
    {
        m_authServerThread.join();
    }
    m_authServer.reset();
}

bool Auth::exchangeCode(const std::string &code)
{
    auto cli = HttpClient::getClient("https://accounts.spotify.com");

    httplib::Headers headers = {
        httplib::make_basic_authentication_header(m_clientId, m_clientSecret),
    };
    httplib::Params params = {
        {"grant_type", "authorization_code"},
        {"code", code},
        {"redirect_uri", m_redirectUri},
    };

    auto res = cli->Post("/api/token", headers, params);
    if (!res || res->status != 200)
    {
        Logger::error("Code exchange failed. HTTP: " + std::to_string(res ? res->status : 0));
        return false;
    }

    json data;
    try
    {
        data = json::parse(res->body);
    }
    catch (const std::exception &e)
    {
        Logger::error(std::string("Failed to parse token exchange response: ") + e.what());
        return false;
    }

    std::string newAccessToken = data.value("access_token", "");
    std::string newRefreshToken = data.value("refresh_token", "");
    int expiresIn = data.value("expires_in", 3600);

    if (newAccessToken.empty())
    {
        Logger::error("Token exchange response missing access_token.");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_tokenMutex);
        m_accessToken = std::move(newAccessToken);
        if (!newRefreshToken.empty())
        {
            m_refreshToken = std::move(newRefreshToken);
        }
        else
        {
            Logger::debug("Token exchange response omitted refresh_token; keeping previous value.");
        }
        m_tokenExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(expiresIn - 60);
    }

    Logger::info("Authentication successful.");
    return true;
}

Auth::RefreshResult Auth::refreshToken()
{
    std::string refreshTokenValue;
    std::string clientId = m_clientId;
    std::string clientSecret = m_clientSecret;
    {
        std::lock_guard<std::mutex> lock(m_tokenMutex);
        if (m_refreshToken.empty()) return RefreshResult::Invalid;
        refreshTokenValue = m_refreshToken;
    }

    auto cli = HttpClient::getClient("https://accounts.spotify.com");
    httplib::Headers headers = {
        httplib::make_basic_authentication_header(clientId, clientSecret)
    };
    httplib::Params params = {
        {"grant_type", "refresh_token"},
        {"refresh_token", refreshTokenValue}
    };

    httplib::Result res;
    try
    {
        res = cli->Post("/api/token", headers, params);
    }
    catch (const std::exception &e)
    {
        Logger::error(std::string("Token refresh threw exception: ") + e.what());
        return RefreshResult::Failed;
    }

    if (!res)
    {
        Logger::error("Token refresh failed (no response).");
        return RefreshResult::Failed;
    }

    if (res->status == 400 || res->status == 401)
    {
        Logger::error("Token refresh rejected by Spotify (HTTP " +
                      std::to_string(res->status) + "); refresh token is no longer valid.");
        return RefreshResult::Invalid;
    }

    if (res->status != 200)
    {
        Logger::error("Token refresh failed. HTTP: " + std::to_string(res->status));
        return RefreshResult::Failed;
    }

    json data;
    try
    {
        data = json::parse(res->body);
    }
    catch (const std::exception &e)
    {
        Logger::error(std::string("Failed to parse refresh token response: ") + e.what());
        return RefreshResult::Failed;
    }

    std::string newAccess = data.value("access_token", "");
    if (newAccess.empty())
    {
        Logger::error("Refresh response missing access_token.");
        return RefreshResult::Failed;
    }

    {
        std::lock_guard<std::mutex> lock(m_tokenMutex);
        m_accessToken = std::move(newAccess);
        int expiresIn = data.value("expires_in", 3600);
        m_tokenExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(expiresIn - 60);
    }

    Logger::debug("Token refreshed successfully.");
    return RefreshResult::Success;
}

void Auth::clearRefreshToken()
{
    std::lock_guard<std::mutex> lock(m_tokenMutex);
    m_refreshToken.clear();
    m_accessToken.clear();
    m_tokenExpiry = std::chrono::steady_clock::time_point{};
}

std::string Auth::getAccessToken()
{
    {
        std::lock_guard<std::mutex> lock(m_tokenMutex);
        if (std::chrono::steady_clock::now() < m_tokenExpiry && !m_accessToken.empty())
        {
            return m_accessToken;
        }
    }

    Logger::debug("Token expired. Refreshing...");
    auto result = refreshToken();
    if (result == RefreshResult::Success)
    {
        std::lock_guard<std::mutex> lock(m_tokenMutex);
        return m_accessToken;
    }

    return "";
}

bool Auth::isAuthenticated() const
{
    std::lock_guard<std::mutex> lock(m_tokenMutex);
    return !m_accessToken.empty();
}

bool Auth::ensureFreshToken()
{
    {
        std::lock_guard<std::mutex> lock(m_tokenMutex);
        if (std::chrono::steady_clock::now() < m_tokenExpiry && !m_accessToken.empty())
        {
            return true;
        }
    }

    auto result = refreshToken();
    if (result == RefreshResult::Success) return true;

    if (result == RefreshResult::Invalid)
    {
        clearRefreshToken();
        return ensureValidSession();
    }

    return false;
}

bool Auth::ensureValidSession()
{
    {
        std::lock_guard<std::mutex> lock(m_tokenMutex);
        if (!m_refreshToken.empty() &&
            std::chrono::steady_clock::now() < m_tokenExpiry &&
            !m_accessToken.empty())
        {
            return true;
        }
    }

    bool alreadyRunning = m_reauthPending.exchange(true);
    if (alreadyRunning)
    {
        Logger::info("Re-authentication already in progress; waiting for completion...");
    }
    else
    {
        {
            std::lock_guard<std::mutex> lock(m_reauthMutex);
            m_reauthState = ReauthState::InProgress;
        }
        Logger::warn("Refresh token expired or revoked. Initiating browser re-authentication...");
        bool ok = performReauth(300);
        {
            std::lock_guard<std::mutex> lock(m_reauthMutex);
            m_reauthState = ok ? ReauthState::Succeeded : ReauthState::Failed;
        }
        m_reauthPending.store(false);
        m_reauthCv.notify_all();

        if (ok)
        {
            Logger::info("Re-authentication succeeded.");
        }
        else
        {
            Logger::error("Re-authentication failed. Hotkeys remain disabled until a successful login.");
        }
        return ok;
    }

    std::unique_lock<std::mutex> lock(m_reauthMutex);
    m_reauthCv.wait(lock, [this]()
    {
        return m_reauthState == ReauthState::Succeeded ||
               m_reauthState == ReauthState::Failed;
    });
    return m_reauthState == ReauthState::Succeeded;
}

void Auth::saveConfig()
{
    AppConfig appConfig;
    appConfig.clientId = m_clientId;
    appConfig.clientSecret = m_clientSecret;
    {
        std::lock_guard<std::mutex> lock(m_tokenMutex);
        appConfig.refreshToken = m_refreshToken;
    }
    m_config.save(appConfig);
}
