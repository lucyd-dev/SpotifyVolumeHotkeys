#pragma once

#include <chrono>
#include <string>
#include "HiddenWindow.hpp"
#include "../core/Config.hpp"
#include "../auth/Auth.hpp"
#include "../volume/Volume.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class AppController
{
public:
    static const UINT HOTKEY_VOL_DOWN = 1;
    static const UINT HOTKEY_VOL_UP = 2;
    static const UINT INPUT_TIMER_INTERVAL = 50;
    static const UINT PLAYER_TIMER_INTERVAL = 1000;
    static const int VOLUME_CHANGE_STEP = 2;

    AppController(AppConfig config, Auth &auth);

    bool startup(HINSTANCE hInstance);
    int run();
    void shutdown();

    HWND handle() const noexcept { return m_window.handle(); }
    bool handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    static UINT vkFromKeyName(const std::string &name);

    void registerHotkeys();
    void setPlayerTimer();
    void volumeChange(int change);
    void updateCurrentVolume();
    void onHotkey(WPARAM wParam);
    void onInputTimer();
    void onPlayerTimer();

    HiddenWindow m_window;
    AppConfig m_appConfig;
    Auth &m_auth;
    VolumeControl m_volume;
    UINT_PTR m_playerTimerId;
    UINT_PTR m_inputTimerId;
    int m_currentVolume;
    int m_pendingVolume;
    std::chrono::steady_clock::time_point m_lastChange;
    bool m_hotkeyDownRegistered;
    bool m_hotkeyUpRegistered;
};