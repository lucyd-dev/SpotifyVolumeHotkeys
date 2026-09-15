#include "AppController.hpp"
#include "HiddenWindow.hpp"
#include "HotkeyMap.hpp"
#include "Autostart.hpp"
#include "../core/Logger.hpp"
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <chrono>
#include <string>

AppController::AppController(AppConfig config, Auth &auth)
    : m_appConfig(std::move(config)),
      m_auth(auth),
      m_volume(auth),
      m_currentVolume(-1),
      m_pendingVolume(0),
      m_lastChange(std::chrono::steady_clock::time_point{}),
      m_hotkeyDownRegistered(false),
      m_hotkeyUpRegistered(false),
      m_autostartEnabled(false),
      m_lastConfigMtime(std::filesystem::file_time_type{}),
      m_exitAction(ExitAction::Exit)
{
}

bool AppController::startup(HINSTANCE hInstance)
{
    if (!m_window.create(hInstance, this))
    {
        Logger::fatal("Failed to create the hidden message window.");
        return false;
    }

    m_lastConfigMtime = m_config.lastWriteTime();
    m_autostartEnabled = Autostart::isEnabled();

    if (m_appConfig.autostart && !m_autostartEnabled)
    {
        if (Autostart::setEnabled(true))
        {
            m_autostartEnabled = true;
            Logger::info("Autostart enabled to match the config file.");
        }
    }

    applyHotkeys();
    SetTimer(m_window.handle(), TIMER_PLAYER, m_appConfig.playerTimerInterval, NULL);
    SetTimer(m_window.handle(), TIMER_CONFIG, CONFIG_TIMER_INTERVAL, NULL);

    m_tray.configure(m_window.handle(), hInstance, WM_TRAY_CALLBACK);
    if (!m_tray.add())
    {
        Logger::fatal("Failed to add the tray icon.");
    }

    updateCurrentVolume();
    updateTrayStatus();
    return true;
}

AppController::ExitAction AppController::run()
{
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return m_exitAction;
}

void AppController::shutdown()
{
    KillTimer(m_window.handle(), TIMER_INPUT);
    KillTimer(m_window.handle(), TIMER_PLAYER);
    KillTimer(m_window.handle(), TIMER_CONFIG);
    m_inputDebounceActive = false;
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

    m_tray.remove();
    m_window.destroy();
}

bool AppController::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hwnd;

    switch (msg)
    {
    case WM_HOTKEY:
        onHotkey(wParam);
        return true;

    case WM_TIMER:
        switch (wParam)
        {
        case TIMER_INPUT:
            onInputTimer();
            return true;
        case TIMER_PLAYER:
            onPlayerTimer();
            return true;
        case TIMER_CONFIG:
            onConfigTimer();
            return true;
        }
        return false;

    case WM_TRAY_CALLBACK:
        onTrayCallback(lParam);
        return true;

    default:
        return false;
    }
}

void AppController::applyHotkeys()
{
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

    registerHotkey(m_appConfig.volumeDownKey, HOTKEY_VOL_DOWN);
    registerHotkey(m_appConfig.volumeUpKey, HOTKEY_VOL_UP);
}

void AppController::registerHotkey(const std::string &name, const UINT id)
{
    std::string reason;
    if (!HotkeyMap::validate(name, reason))
    {
        Logger::warn("Invalid hotkey: " + reason);
        return;
    }

    if (RegisterHotKey(m_window.handle(), id, 0,
                            HotkeyMap::toVk(name)))
    {
        if (id == HOTKEY_VOL_DOWN)
        {
            m_hotkeyDownRegistered = true;
        }
        else
        {
            m_hotkeyUpRegistered = true;
        }
        return;
    }

    Logger::warn("Hotkey " + name + " in use by another application");
}

void AppController::onHotkey(WPARAM wParam)
{
    int id = (int)wParam;
    int step = (id == HOTKEY_VOL_UP) ? VOLUME_CHANGE_STEP : -VOLUME_CHANGE_STEP;

    m_pendingVolume += step;
    m_inputDebounceActive = true;
    SetTimer(m_window.handle(), TIMER_INPUT, m_appConfig.inputTimerInterval, NULL);
}

void AppController::updateCurrentVolume()
{
    int playerVolume = m_volume.getPlayerVolume();
    m_currentVolume = playerVolume;
}

void AppController::updateTrayStatus()
{
    std::wstring status;
    if (m_volume.isRateLimited())
    {
        status = L"Rate limited";
    }
    else if (!m_volume.isPlayerActive())
    {
        status = L"No active device";
    }
    else if (m_currentVolume >= 0)
    {
        status = L"Player active \u00B7 " + std::to_wstring(m_currentVolume) + L"%";
    }
    else
    {
        status = L"Player active";
    }

    if (status != m_statusText)
    {
        m_statusText = status;
        m_tray.setStatus(status);
    }
}

void AppController::onInputTimer()
{
    KillTimer(m_window.handle(), TIMER_INPUT);
    m_inputDebounceActive = false;

    if (!m_volume.isPlayerActive() || m_volume.isRateLimited() || m_currentVolume < 0)
    {
        m_pendingVolume = 0;
        return;
    }

    int newVolume = std::clamp(m_currentVolume + m_pendingVolume, 0, 100);
    if (m_volume.setPlayerVolume(newVolume))
    {
        m_currentVolume = newVolume;
        m_lastChange = std::chrono::steady_clock::now();
        updateTrayStatus();
    }

    m_pendingVolume = 0;
}

void AppController::onPlayerTimer()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastChange).count();
    if (elapsed >= 1000)
    {
        updateCurrentVolume();
        updateTrayStatus();
    }
}

void AppController::onConfigTimer()
{
    auto mtime = m_config.lastWriteTime();
    if (mtime == std::filesystem::file_time_type{} || mtime == m_lastConfigMtime)
    {
        return;
    }
    m_lastConfigMtime = mtime;
    applyConfigReload();
}

void AppController::applyConfigReload()
{
    AppConfig fresh = m_config.load();

    if (!m_config.lastLoadSuccess() ||
        (fresh.clientId.empty() && fresh.clientSecret.empty()))
    {
        Logger::warn("Config file unparsable or missing credentials; "
                     "keeping the running configuration.");
        return;
    }

    bool hotkeysChanged = fresh.volumeDownKey != m_appConfig.volumeDownKey ||
                          fresh.volumeUpKey != m_appConfig.volumeUpKey;
    bool credsChanged = fresh.clientId != m_appConfig.clientId ||
                        fresh.clientSecret != m_appConfig.clientSecret;
    bool autostartChanged = fresh.autostart != m_appConfig.autostart;
    bool intervalsChanged = fresh.inputTimerInterval != m_appConfig.inputTimerInterval ||
                            fresh.playerTimerInterval != m_appConfig.playerTimerInterval;

    if (hotkeysChanged)
    {
        m_appConfig.volumeDownKey = fresh.volumeDownKey;
        m_appConfig.volumeUpKey = fresh.volumeUpKey;
        applyHotkeys();
    }

    if (intervalsChanged)
    {
        m_appConfig.inputTimerInterval = fresh.inputTimerInterval;
        m_appConfig.playerTimerInterval = fresh.playerTimerInterval;

        KillTimer(m_window.handle(), TIMER_PLAYER);
        SetTimer(m_window.handle(), TIMER_PLAYER, m_appConfig.playerTimerInterval, NULL);

        if (m_inputDebounceActive)
        {
            KillTimer(m_window.handle(), TIMER_INPUT);
            SetTimer(m_window.handle(), TIMER_INPUT, m_appConfig.inputTimerInterval, NULL);
        }
    }

    if (credsChanged)
    {
        m_auth.applyConfig(fresh);
        m_appConfig.clientId = fresh.clientId;
        m_appConfig.clientSecret = fresh.clientSecret;
        m_appConfig.refreshToken = fresh.refreshToken;
    }

    if (autostartChanged)
    {
        if (Autostart::setEnabled(fresh.autostart))
        {
            m_appConfig.autostart = fresh.autostart;
            m_autostartEnabled = fresh.autostart;
        }
    }

    if (hotkeysChanged || credsChanged || autostartChanged || intervalsChanged)
    {
        Logger::info("Config file changed; credentials, hotkeys, autostart and polling intervals reloaded.");
    }
}

void AppController::onTrayCallback(LPARAM lParam)
{
    UINT event = LOWORD(lParam);
    if (event == WM_LBUTTONUP  || event == WM_RBUTTONUP)
    {
        int cmd = m_tray.showPopup(m_statusText, m_autostartEnabled);
        if (cmd > 0)
        {
            handleMenuCommand(cmd);
        }
    }
}

void AppController::handleMenuCommand(int cmd)
{
    switch (cmd)
    {
    case TrayIcon::MenuEditConfig:
        openConfigInEditor();
        break;
    case TrayIcon::MenuOpenLogs:
        openLogsInEditor();
        break;
    case TrayIcon::MenuToggleAutostart:
        toggleAutostart();
        break;
    case TrayIcon::MenuRestart:
        restartApp();
        break;
    case TrayIcon::MenuExit:
        requestExit();
        break;
    default:
        break;
    }
}

void AppController::openConfigInEditor()
{
    std::wstring cfg = m_config.path().wstring();
    ShellExecuteW(NULL, L"open", cfg.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void AppController::openLogsInEditor()
{
    std::wstring logStr(Logger::utf16(Logger::getLogFilePath()).data());
    ShellExecuteW(NULL, L"open", logStr.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void AppController::toggleAutostart()
{
    bool newValue = !m_autostartEnabled;
    if (!Autostart::setEnabled(newValue))
    {
        return;
    }

    m_autostartEnabled = newValue;
    m_appConfig.autostart = newValue;
    if (m_config.save(m_appConfig))
    {
        m_lastConfigMtime = m_config.lastWriteTime();
    }
    Logger::info(std::string("Autostart ") +
                 (m_autostartEnabled ? "enabled" : "disabled") + ".");
}

void AppController::restartApp()
{
    m_exitAction = ExitAction::Restart;
    PostQuitMessage(0);
}

void AppController::requestExit()
{
    m_exitAction = ExitAction::Exit;
    PostQuitMessage(0);
}
