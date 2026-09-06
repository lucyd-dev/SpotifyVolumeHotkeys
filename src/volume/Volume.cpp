#include "Volume.hpp"
#include "../core/HttpClient.hpp"
#include "../core/Logger.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <functional>

using json = nlohmann::json;

namespace
{
    int parseRetryAfterSeconds(const std::string &value)
    {
        if (value.empty()) return 0;
        try
        {
            return std::stoi(value);
        }
        catch (...)
        {
            return 0;
        }
    }

    std::string formatClock(std::chrono::steady_clock::time_point)
    {
        auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        return oss.str();
    }
}

VolumeControl::VolumeControl(Auth &auth)
    : m_auth(auth)
{
}

httplib::Headers VolumeControl::getHeaders()
{
    return httplib::Headers{
        {"Authorization", "Bearer " + m_auth.getAccessToken()}
    };
}

void VolumeControl::logInactiveOnce(const char *context)
{
    bool expected = false;
    if (m_inactiveLogged.compare_exchange_strong(expected, true))
    {
        Logger::info(std::string("No active Spotify playback device (") + context + "); "
                      "volume hotkeys disabled until a device is available.");
    }
}

void VolumeControl::markActive(int currentVolume)
{
    bool wasInactive = !m_playerActive.exchange(true);
    m_inactiveLogged.store(false);
    if (wasInactive)
    {
        bool expected = false;
        if (m_recoveredLogged.compare_exchange_strong(expected, true))
        {
            if (currentVolume >= 0)
            {
                Logger::info("Spotify playback device available (current volume " +
                             std::to_string(currentVolume) + "%); volume hotkeys enabled.");
            }
            else
            {
                Logger::info("Spotify playback device available; volume hotkeys enabled.");
            }
        }
    }
}

void VolumeControl::markInactive(const char *context)
{
    if (m_playerActive.exchange(false))
    {
        m_recoveredLogged.store(false);
    }
    logInactiveOnce(context);
}

bool VolumeControl::isRateLimited() const
{
    using namespace std::chrono;
    steady_clock::time_point until;
    {
        std::lock_guard<std::mutex> lock(m_rateLimitMutex);
        until = m_rateLimitUntil;
    }
    return steady_clock::now() < until;
}

void VolumeControl::applyRateLimitHeader(const httplib::Result &res, const char *endpoint)
{
    if (!res) return;

    int retryAfter = 0;
    auto it = res->headers.find("Retry-After");
    if (it != res->headers.end())
    {
        retryAfter = parseRetryAfterSeconds(it->second);
    }
    if (retryAfter <= 0) retryAfter = 5;
    if (retryAfter < 1) retryAfter = 1;

    using namespace std::chrono;
    auto candidate = steady_clock::now() + seconds(retryAfter);

    steady_clock::time_point prev;
    {
        std::lock_guard<std::mutex> lock(m_rateLimitMutex);
        if (candidate > m_rateLimitUntil)
        {
            m_rateLimitUntil = candidate;
        }
        prev = m_rateLimitUntil;
    }

    Logger::warn(std::string("429 Too Many Requests from ") + endpoint +
                 "; honoring Retry-After: " + std::to_string(retryAfter) +
                 "s. All Spotify calls delayed until " + formatClock(prev) + ".");
}

VolumeControl::RequestResult VolumeControl::runWithRecovery(
    std::function<httplib::Result()> build,
    const char *endpoint)
{
    if (isRateLimited())
    {
        Logger::debug(std::string("Skipping request to ") + endpoint + ": rate-limit window active.");
        return {httplib::Result{}, Outcome::Deactivate};
    }

    httplib::Result res = build();

    if (res && res->status == 401)
    {
        Logger::debug(std::string("") + endpoint + ": refreshing access token after 401 and retrying.");
        if (m_auth.ensureValidSession())
        {
            res = build();
        }
    }

    if (res && res->status == 429)
    {
        applyRateLimitHeader(res, endpoint);
        return {std::move(res), Outcome::Deactivate};
    }

    if (!res)
    {
        Logger::error(std::string("") + endpoint + " failed (no response).");
        return {std::move(res), Outcome::Deactivate};
    }

    if (res->status == 401)
    {
        Logger::error(std::string("") + endpoint + ": 401 after token refresh; "
                      "check client_id/client_secret.");
        return {std::move(res), Outcome::Deactivate};
    }

    if (res->status == 403)
    {
        Logger::error(std::string("403 Forbidden from ") + endpoint + "; "
                      "check that your client_id/client_secret are correct and that the Spotify app has the required scopes.");
        return {std::move(res), Outcome::Deactivate};
    }

    if (res->status == 404)
    {
        return {std::move(res), Outcome::Deactivate};
    }

    if (res->status < 200 || res->status >= 300)
    {
        Logger::error(std::string("") + endpoint + " failed. HTTP: " + std::to_string(res->status));
        return {std::move(res), Outcome::Deactivate};
    }

    return {std::move(res), Outcome::Success};
}

int VolumeControl::getPlayerVolume()
{
    auto build = [&]() -> httplib::Result
    {
        auto cli = HttpClient::getClient("https://api.spotify.com");
        return cli->Get("/v1/me/player", getHeaders());
    };

    auto result = runWithRecovery(build, "getPlayerVolume");

    if (result.outcome == Outcome::Deactivate)
    {
        markInactive("getPlayerVolume");
        return -1;
    }

    const auto &res = result.response;
    if (res->status == 204)
    {
        markInactive("getPlayerVolume");
        return -1;
    }

    json data;
    try
    {
        data = json::parse(res->body);
    }
    catch (const std::exception &e)
    {
        Logger::error(std::string("Failed to parse player state JSON: ") + e.what());
        markInactive("getPlayerVolume");
        return -1;
    }

    if (!data.contains("device") || data["device"].is_null())
    {
        markInactive("getPlayerVolume");
        return -1;
    }

    const auto &device = data["device"];

    bool isActive       = device.value("is_active", false);
    bool isRestricted   = device.value("is_restricted", false);
    bool supportsVolume = device.value("supports_volume", false);
    int  volume         = device.value("volume_percent", -1);

    if (!isActive || isRestricted || !supportsVolume || volume < 0)
    {
        Logger::warn(std::string("Device not controllable: is_active=") +
                     (isActive ? "true" : "false") +
                     ", is_restricted=" + (isRestricted ? "true" : "false") +
                     ", supports_volume=" + (supportsVolume ? "true" : "false") +
                     ", volume_percent=" + std::to_string(volume));
        markInactive("getPlayerVolume");
        return -1;
    }

    markActive(volume);
    return volume;
}

bool VolumeControl::isPlayerActive()
{
    return m_playerActive.load();
}

bool VolumeControl::setPlayerVolume(int volume)
{
    if (!m_playerActive.load())
    {
        logInactiveOnce("setPlayerVolume");
        return false;
    }

    volume = std::clamp(volume, 0, 100);
    int requestedVolume = volume;

    auto build = [&]() -> httplib::Result
    {
        auto cli = HttpClient::getClient("https://api.spotify.com");
        std::string path = "/v1/me/player/volume?volume_percent=" + std::to_string(volume);
        return cli->Put(path.c_str(), getHeaders(), "", "application/json");
    };

    auto result = runWithRecovery(build, "setPlayerVolume");

    if (result.outcome == Outcome::Deactivate)
    {
        markInactive("setPlayerVolume");
        return false;
    }

    if (result.response->status == 204)
    {
        Logger::debug("setPlayerVolume: volume changed to " + std::to_string(requestedVolume) + "%");
        return true;
    }

    return false;
}
