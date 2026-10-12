#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include "HiddenWindow.hpp"
#include "TrayIcon.hpp"
#include "KeyboardHook.hpp"
#include "../core/Config.hpp"
#include "../core/UpdateChecker.hpp"
#include "../auth/Auth.hpp"
#include "../volume/Volume.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Startup knobs, mainly for testing the update flow without publishing a release.
struct AppOptions
{
    // When non-empty, skip the network and pretend this version is available.
    std::string simulateUpdateVersion;
    // Run a check immediately on startup and surface its result even if nothing changed.
    bool forceCheck = false;
};

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
    static const UINT WM_APP_HOOK_KEY = WM_APP + 2;
    static const UINT WM_APP_UPDATE_RESULT = WM_APP + 3;
    static const UINT WM_APP_INSTALL_RESULT = WM_APP + 4;
    static const int VOLUME_CHANGE_STEP = 2;

    AppController(AppConfig config, Auth &auth, AppOptions options = {});

    bool startup(HINSTANCE hInstance);
    ExitAction run();
    void shutdown();

    HWND handle() const noexcept { return m_window.handle(); }
    bool handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void applyHotkeys();
    void updateCurrentVolume();
    void updateTrayStatus();
    void triggerVolume(int step);
    void onHotkey(WPARAM wParam);
    void onHookKey(WPARAM wParam);
    void onInputTimer();
    void onPlayerTimer();
    void onConfigTimer();
    void onTrayCallback(LPARAM lParam);
    void applyConfigReload();
    void handleMenuCommand(int cmd);
    void openConfigInEditor();
    void openLogsInEditor();
    void toggleAutostart();
    void restartApp();
    void requestExit();

    void startUpdateCheck(bool manual);
    void onUpdateResult();
    void showUpdatePrompt();
    void openReleasePage();
    std::wstring updateMenuLabel() const;
    bool canSelfInstall() const;
    void startInstall();
    void onInstallResult();

    HiddenWindow m_window;
    TrayIcon m_tray;
    KeyboardHook m_keyboardHook;
    AppConfig m_appConfig;
    AppOptions m_options;
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

    std::thread m_updateThread;
    std::mutex m_updateMutex;
    std::atomic<bool> m_updateRunning{false};
    std::atomic<bool> m_updateStop{false};
    UpdateChecker::Result m_updateResult;
    bool m_manualCheck = false;
    bool m_updateAvailable = false;
    bool m_updateDismissed = false;
    std::string m_updateLatest;
    std::string m_updateUrl;
    std::string m_installerUrl;
    std::string m_installerSha256;

    std::thread m_installThread;
    std::mutex m_installMutex;
    std::atomic<bool> m_installRunning{false};
    bool m_installOk = false;
    std::string m_installError;
};
