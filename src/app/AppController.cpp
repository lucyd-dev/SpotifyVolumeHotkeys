#include "AppController.hpp"
#include "HiddenWindow.hpp"
#include "../core/Logger.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <string>

AppController::AppController(AppConfig config, Auth &auth)
    : m_appConfig(std::move(config)),
      m_auth(auth),
      m_volume(auth),
      m_playerTimerId(0),
      m_inputTimerId(0),
      m_currentVolume(-1),
      m_pendingVolume(0),
      m_lastChange(std::chrono::steady_clock::time_point{}),
      m_hotkeyDownRegistered(false),
      m_hotkeyUpRegistered(false)
{
}

UINT AppController::vkFromKeyName(const std::string &name)
{
    std::string upper;
    upper.reserve(name.size());
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        char c = name[i];
        if (c >= 'a' && c <= 'z')
        {
            c = (char)(c - ('a' - 'A'));
        }
        upper += c;
    }

    if (upper == "SPACE") return VK_SPACE;
    if (upper == "UP")    return VK_UP;
    if (upper == "DOWN")  return VK_DOWN;
    if (upper == "LEFT")  return VK_LEFT;
    if (upper == "RIGHT") return VK_RIGHT;

    if (upper.size() == 1 && upper[0] >= 'A' && upper[0] <= 'Z')
    {
        return (UINT)upper[0];
    }
    if (upper.size() == 1 && upper[0] >= '0' && upper[0] <= '9')
    {
        return (UINT)upper[0];
    }

    if (upper.size() >= 2 && upper[0] == 'F')
    {
        int num = 0;
        std::size_t i;
        for (i = 1; i < upper.size(); ++i)
        {
            if (upper[i] < '0' || upper[i] > '9') break;
            num = num * 10 + (upper[i] - '0');
        }
        if (i == upper.size() && num >= 1 && num <= 24)
        {
            return (UINT)(VK_F1 + num - 1);
        }
    }

    return 0;
}

bool AppController::startup(HINSTANCE hInstance)
{
    if (!m_window.create(hInstance, this))
    {
        Logger::fatal("Failed to create the hidden message window.");
        return false;
    }

    registerHotkeys();
    setPlayerTimer();
    updateCurrentVolume();
    return true;
}

int AppController::run()
{
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

void AppController::shutdown()
{
    if (m_inputTimerId)
    {
        KillTimer(m_window.handle(), m_inputTimerId);
        m_inputTimerId = 0;
    }
    if (m_playerTimerId)
    {
        KillTimer(m_window.handle(), m_playerTimerId);
        m_playerTimerId = 0;
    }
    m_pendingVolume = 0;

    if (m_hotkeyDownRegistered)
    {
        UnregisterHotKey(m_window.handle(), HOTKEY_VOL_DOWN);
        m_hotkeyDownRegistered = false;
    }
    if (m_hotkeyUpRegistered)
    {
        UnregisterHotKey(m_window.handle(), HOTKEY_VOL_UP);
        m_hotkeyUpRegistered = false;
    }

    m_window.destroy();
}

bool AppController::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hwnd;
    (void)lParam;

    switch (msg)
    {
    case WM_HOTKEY:
        onHotkey(wParam);
        return true;

    case WM_TIMER:
        if (wParam == m_inputTimerId)
        {
            onInputTimer();
            return true;
        }
        if (wParam == m_playerTimerId)
        {
            onPlayerTimer();
            return true;
        }
        return false;

    default:
        return false;
    }
}

void AppController::registerHotkeys()
{
    UINT downVK = vkFromKeyName(m_appConfig.volumeDownKey);
    UINT upVK = vkFromKeyName(m_appConfig.volumeUpKey);

    if (downVK != 0 && RegisterHotKey(m_window.handle(), HOTKEY_VOL_DOWN, 0, downVK))
    {
        m_hotkeyDownRegistered = true;
    }
    else
    {
        Logger::warn("Failed to register volume-down hotkey (" +
                     m_appConfig.volumeDownKey + ").");
    }

    if (upVK != 0 && RegisterHotKey(m_window.handle(), HOTKEY_VOL_UP, 0, upVK))
    {
        m_hotkeyUpRegistered = true;
    }
    else
    {
        Logger::warn("Failed to register volume-up hotkey (" +
                     m_appConfig.volumeUpKey + ").");
    }
}

void AppController::onHotkey(WPARAM wParam)
{
    int id = (int)wParam;
    if (id == HOTKEY_VOL_DOWN)
    {
        volumeChange(-VOLUME_CHANGE_STEP);
    }
    else if (id == HOTKEY_VOL_UP)
    {
        volumeChange(VOLUME_CHANGE_STEP);
    }
}

void AppController::volumeChange(int change)
{
    m_pendingVolume += change;
    m_inputTimerId = SetTimer(m_window.handle(), m_inputTimerId,
                              INPUT_TIMER_INTERVAL, NULL);
}

void AppController::setPlayerTimer()
{
    m_playerTimerId = SetTimer(m_window.handle(), m_playerTimerId,
                               PLAYER_TIMER_INTERVAL, NULL);
}

void AppController::updateCurrentVolume()
{
    int playerVolume = m_volume.getPlayerVolume();
    if (playerVolume != -1)
    {
        m_currentVolume = playerVolume;
    }
}

void AppController::onInputTimer()
{
    KillTimer(m_window.handle(), m_inputTimerId);
    m_inputTimerId = 0;

    if (!m_volume.isPlayerActive() || m_volume.isRateLimited())
    {
        m_pendingVolume = 0;
    }
    else if (m_currentVolume >= 0)
    {
        int newVolume = m_currentVolume + m_pendingVolume;
        if (m_volume.setPlayerVolume(newVolume))
        {
            m_currentVolume = newVolume;
            m_pendingVolume = 0;
            m_lastChange = std::chrono::steady_clock::now();
        }
    }
}

void AppController::onPlayerTimer()
{
    KillTimer(m_window.handle(), m_playerTimerId);
    m_playerTimerId = 0;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastChange).count();

    if (elapsed >= 2000)
    {
        updateCurrentVolume();
    }

    if (m_volume.isRateLimited() && m_pendingVolume != 0 &&
        m_volume.isPlayerActive() && m_currentVolume >= 0)
    {
        int newVolume = m_currentVolume + m_pendingVolume;
        newVolume = std::clamp(newVolume, 0, 100);
        if (m_volume.setPlayerVolume(newVolume))
        {
            m_currentVolume = newVolume;
            m_pendingVolume = 0;
            m_lastChange = std::chrono::steady_clock::now();
        }
    }

    setPlayerTimer();
}