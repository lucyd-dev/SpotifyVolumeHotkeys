#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include "HiddenWindow.hpp"
#include "TrayIcon.hpp"
#include "../core/Config.hpp"
#include "../auth/Auth.hpp"
#include "../volume/Volume.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class AppController
{
public:
    enum class ExitAction
    {
        Exit,
        Restart,
    };

    static const UINT HOTKEY_VOL_DOWN = 1;
    static const UINT HOTKEY_VOL_UP = 2;
    static constexpr UINT_PTR TIMER_INPUT = 1;
    static constexpr UINT_PTR TIMER_PLAYER = 2;
    static constexpr UINT_PTR TIMER_CONFIG = 3;
    static const UINT CONFIG_TIMER_INTERVAL = 2000;
    static const UINT WM_TRAY_CALLBACK = WM_APP + 1;
    static const int VOLUME_CHANGE_STEP = 2;

    AppController(AppConfig config, Auth &auth);

    bool startup(HINSTANCE hInstance);
    ExitAction run();
    void shutdown();

    HWND handle() const noexcept { return m_window.handle(); }
    bool handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void applyHotkeys();
    void registerHotkey(const std::string &name, const UINT id);
    void updateCurrentVolume();
    void updateTrayStatus();
    void onHotkey(WPARAM wParam);
    void onInputTimer();
    void onPlayerTimer();
    void onConfigTimer();
    void onTrayCallback(LPARAM lParam);
    void applyConfigReload();
    void handleMenuCommand(int cmd);
    void openConfigInEditor();
    void openLogsFolder();
    void toggleAutostart();
    void restartApp();
    void requestExit();

    HiddenWindow m_window;
    TrayIcon m_tray;
    AppConfig m_appConfig;
    Config m_config;
    Auth &m_auth;
    VolumeControl m_volume;
    int m_currentVolume;
    int m_pendingVolume;
    std::chrono::steady_clock::time_point m_lastChange;
    bool m_inputDebounceActive = false;
    bool m_hotkeyDownRegistered;
    bool m_hotkeyUpRegistered;
    std::wstring m_statusText;
    bool m_autostartEnabled;
    std::filesystem::file_time_type m_lastConfigMtime;
    ExitAction m_exitAction;
};
