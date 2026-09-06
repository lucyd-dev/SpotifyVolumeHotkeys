#include <winsock2.h>
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <string>
#include <chrono>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "core/Logger.hpp"
#include "core/HttpClient.hpp"
#include "auth/Auth.hpp"
#include "volume/Volume.hpp"

using json = nlohmann::json;

const std::string REDIRECT_URI = "http://127.0.0.1:8888/callback";
const std::string LOG_FILE = "spotify_volume_hotkeys.log";

constexpr int HOTKEY_VOL_DOWN = 1;
constexpr int HOTKEY_VOL_UP   = 2;

UINT_PTR g_inputTimerId = 0;
int g_InputTimerInterval = 50;
UINT_PTR g_playerTimerId = 1;
int g_PlayerTimerInterval = 1000;
int g_volumeChangeValue = 2;
int g_pendingVolume = 0;
int g_currentVolume = -1;
std::chrono::steady_clock::time_point last_change{};

void volumeChange(const int change)
{
    g_pendingVolume += change;
    g_inputTimerId = SetTimer(NULL, g_inputTimerId, g_InputTimerInterval, NULL);
}

void setPlayerTimer()
{
    g_playerTimerId = SetTimer(NULL, g_playerTimerId, g_PlayerTimerInterval, NULL);
}

void updateCurrentVolume(VolumeControl &volume)
{
    auto playerVolume = volume.getPlayerVolume();
    if (playerVolume != -1)
    {
        if (g_currentVolume != playerVolume)
        {
            g_currentVolume = playerVolume;
        }
    }
}

int main()
{
    Logger::setLogFile(LOG_FILE);
    Logger::cleanupLogFile();

    if (!HttpClient::init())
        return 1;

    if (!RegisterHotKey(NULL, HOTKEY_VOL_DOWN, 0, VK_F13) ||
        !RegisterHotKey(NULL, HOTKEY_VOL_UP,   0, VK_F14))
    {
        Logger::fatal("Failed to register hotkeys.");
        return 1;
    }

    Logger::info("Initializing authentication...");
    Auth auth(REDIRECT_URI);
    if (!auth.authenticate())
    {
        Logger::fatal("Auth failed.");
        UnregisterHotKey(NULL, HOTKEY_VOL_DOWN);
        UnregisterHotKey(NULL, HOTKEY_VOL_UP);
        return 1;
    }
    VolumeControl volume(auth);
    Logger::info("Authentication successful!");

    Logger::info("App started. Wait for input...");

    setPlayerTimer();
    updateCurrentVolume(volume);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        if (msg.message == WM_HOTKEY)
        {
            int id = (int)msg.wParam;
            if (id == HOTKEY_VOL_DOWN)      volumeChange(-g_volumeChangeValue);
            else if (id == HOTKEY_VOL_UP)  volumeChange(g_volumeChangeValue);
            continue;
        }

        if (msg.message != WM_TIMER)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        if (msg.wParam == g_inputTimerId)
        {
            KillTimer(NULL, g_inputTimerId);
            g_inputTimerId = 0;

            if (!volume.isPlayerActive() || volume.isRateLimited())
            {
                g_pendingVolume = 0;
            }
            else if (g_currentVolume >= 0)
            {
                int newVolume = g_currentVolume + g_pendingVolume;
                if (volume.setPlayerVolume(newVolume))
                {
                    g_currentVolume = newVolume;
                    g_pendingVolume = 0;
                    last_change = std::chrono::steady_clock::now();
                }
            }
        }
        else if (msg.wParam == g_playerTimerId)
        {
            KillTimer(NULL, g_playerTimerId);
            g_playerTimerId = 0;

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_change).count();

            if (elapsed >= 2000) updateCurrentVolume(volume);

            if (volume.isRateLimited() && g_pendingVolume != 0 && volume.isPlayerActive() && g_currentVolume >= 0)
            {
                int newVolume = g_currentVolume + g_pendingVolume;
                newVolume = std::clamp(newVolume, 0, 100);
                if (volume.setPlayerVolume(newVolume))
                {
                    g_currentVolume = newVolume;
                    g_pendingVolume = 0;
                    last_change = std::chrono::steady_clock::now();
                }
            }

            setPlayerTimer();
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterHotKey(NULL, HOTKEY_VOL_DOWN);
    UnregisterHotKey(NULL, HOTKEY_VOL_UP);
    return 0;
}
