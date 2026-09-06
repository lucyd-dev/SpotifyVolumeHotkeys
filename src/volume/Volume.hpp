#pragma once

#include "../core/HttpClient.hpp"
#include "../auth/Auth.hpp"
#include <atomic>
#include <chrono>
#include <mutex>

class VolumeControl
{
public:
    explicit VolumeControl(Auth &auth);

    int getPlayerVolume();
    bool setPlayerVolume(int percent);
    bool isPlayerActive();
    bool isRateLimited() const;

private:
    enum class Outcome
    {
        Success,
        Deactivate,
    };

    struct RequestResult
    {
        httplib::Result response;
        Outcome outcome;
    };

    httplib::Headers getHeaders();
    void logInactiveOnce(const char *context);

    void applyRateLimitHeader(const httplib::Result &res, const char *endpoint);

    RequestResult runWithRecovery(std::function<httplib::Result()> build,
                                  const char *endpoint);

    void markActive(int currentVolume);
    void markInactive(const char *context);

    Auth &m_auth;
    std::atomic<bool> m_playerActive{false};
    std::atomic<bool> m_inactiveLogged{false};
    std::atomic<bool> m_recoveredLogged{false};

    std::chrono::steady_clock::time_point m_rateLimitUntil{};
    mutable std::mutex m_rateLimitMutex;
};
