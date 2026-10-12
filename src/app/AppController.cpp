#include "AppController.hpp"
#include "HiddenWindow.hpp"
#include "HotkeyMap.hpp"
#include "Autostart.hpp"
#include "Version.hpp"
#include "../core/Logger.hpp"
#include "../core/Updater.hpp"
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

namespace
{
    constexpr const char *REPO_OWNER = "lucyd-dev";
    constexpr const char *REPO_NAME = "SpotifyVolumeHotkeys";
    constexpr const char *REPO_URL = "https://github.com/lucyd-dev/SpotifyVolumeHotkeys";
    constexpr const char *RELEASES_URL =
        "https://github.com/lucyd-dev/SpotifyVolumeHotkeys/releases/latest";
    constexpr int UPDATE_SETTLE_STEPS = 30; // 30 x 100ms = ~3s of startup settling
    constexpr int UPDATE_SETTLE_STEP_MS = 100;
}

AppController::AppController(AppConfig config, Auth &auth, AppOptions options)
    : m_appConfig(std::move(config)),
      m_options(std::move(options)),
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

    Logger::info(std::string("SpotifyVolumeHotkeys v") + Version::String);
    startUpdateCheck(m_options.forceCheck);
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
    m_updateStop = true;
    if (m_updateThread.joinable())
    {
        m_updateThread.join();
    }
    if (m_installThread.joinable())
    {
        m_installThread.join();
    }

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

    m_keyboardHook.stop();

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

    case WM_APP_HOOK_KEY:
        onHookKey(wParam);
        return true;

    case WM_APP_UPDATE_RESULT:
        onUpdateResult();
        return true;

    case WM_APP_INSTALL_RESULT:
        onInstallResult();
        return true;

    case WM_CLOSE:
        // Lets the installer's Restart Manager close us cleanly during a manual
        // upgrade while the tray instance is still running.
        requestExit();
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

    m_keyboardHook.stop();

    std::vector<UINT> hookKeys;

    auto applyKey = [&](const std::string &name, const UINT id, bool &registered)
    {
        std::string reason;
        if (!HotkeyMap::validate(name, reason))
        {
            Logger::warn("Invalid hotkey: " + reason);
            return;
        }

        UINT vk = HotkeyMap::toVk(name);

        // Media keys are handled by the low-level hook so they also work while
        // a game holds the keyboard in exclusive mode (where RegisterHotKey is
        // disabled by the system).
        if (HotkeyMap::isMediaKey(vk))
        {
            hookKeys.push_back(vk);
            return;
        }

        if (RegisterHotKey(m_window.handle(), id, 0, vk))
        {
            registered = true;
        }
        else
        {
            Logger::warn("Hotkey " + name + " in use by another application");
        }
    };

    applyKey(m_appConfig.volumeDownKey, HOTKEY_VOL_DOWN, m_hotkeyDownRegistered);
    applyKey(m_appConfig.volumeUpKey, HOTKEY_VOL_UP, m_hotkeyUpRegistered);

    if (!hookKeys.empty())
    {
        m_keyboardHook.setKeys(hookKeys);
        if (m_keyboardHook.start(m_window.handle(), WM_APP_HOOK_KEY))
        {
            Logger::info("Low-level keyboard hook active for native media keys.");
        }
        else
        {
            Logger::warn("Failed to install the low-level keyboard hook; media keys "
                         "may fall back to the Windows master volume in games.");
        }
    }
}

void AppController::triggerVolume(int step)
{
    m_pendingVolume += step;
    m_inputDebounceActive = true;
    SetTimer(m_window.handle(), TIMER_INPUT, m_appConfig.inputTimerInterval, NULL);
}

void AppController::onHotkey(WPARAM wParam)
{
    int id = (int)wParam;
    triggerVolume(id == HOTKEY_VOL_UP ? VOLUME_CHANGE_STEP : -VOLUME_CHANGE_STEP);
}

void AppController::onHookKey(WPARAM wParam)
{
    UINT vk = (UINT)wParam;
    if (vk == HotkeyMap::toVk(m_appConfig.volumeUpKey))
    {
        triggerVolume(VOLUME_CHANGE_STEP);
    }
    else if (vk == HotkeyMap::toVk(m_appConfig.volumeDownKey))
    {
        triggerVolume(-VOLUME_CHANGE_STEP);
    }
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
        TrayIcon::MenuState state;
        state.status = m_statusText;
        state.autostartEnabled = m_autostartEnabled;
        state.updateAvailable = m_updateAvailable;
        state.updateLabel = updateMenuLabel();

        int cmd = m_tray.showPopup(state);
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
    case TrayIcon::MenuOpenGitHub:
    {
        std::vector<wchar_t> url = Logger::utf16(REPO_URL);
        ShellExecuteW(NULL, L"open", url.data(), NULL, NULL, SW_SHOWNORMAL);
        break;
    }
    case TrayIcon::MenuToggleAutostart:
        toggleAutostart();
        break;
    case TrayIcon::MenuCheckUpdate:
        if (m_updateAvailable)
        {
            if (canSelfInstall())
            {
                startInstall();
            }
            else
            {
                openReleasePage();
            }
        }
        else
        {
            startUpdateCheck(true);
        }
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

void AppController::startUpdateCheck(bool manual)
{
    if (m_updateRunning.load())
    {
        return; // a check is already in flight
    }
    if (m_updateThread.joinable())
    {
        m_updateThread.join(); // reap the previous, completed worker
    }

    m_manualCheck = manual;
    m_updateStop = false;
    m_updateRunning = true;

    m_updateThread = std::thread([this]()
    {
        struct RunningGuard
        {
            std::atomic<bool> &flag;
            ~RunningGuard() { flag = false; }
        } guard{m_updateRunning};

        // Let auth/UI settle before touching the network on ordinary startup.
        // Simulated checks skip the wait so testing stays snappy.
        if (m_options.simulateUpdateVersion.empty())
        {
            for (int i = 0; i < UPDATE_SETTLE_STEPS && !m_updateStop.load(); ++i)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_SETTLE_STEP_MS));
            }
            if (m_updateStop.load())
            {
                return;
            }
        }

        UpdateChecker::Result result;
        if (!m_options.simulateUpdateVersion.empty())
        {
            result.status = UpdateChecker::Result::Status::UpdateAvailable;
            result.currentVersion = Version::String;
            result.latestVersion = m_options.simulateUpdateVersion;
            result.releaseUrl = RELEASES_URL;
            Logger::info("Update check simulated: pretending v" +
                         m_options.simulateUpdateVersion + " is available.");
        }
        else
        {
            result = UpdateChecker::checkLatest(REPO_OWNER, REPO_NAME, Version::String);
        }

        {
            std::lock_guard<std::mutex> lock(m_updateMutex);
            m_updateResult = result;
        }

        if (!m_updateStop.load())
        {
            PostMessageW(m_window.handle(), WM_APP_UPDATE_RESULT, 0, 0);
        }
    });
}

void AppController::onUpdateResult()
{
    UpdateChecker::Result result;
    {
        std::lock_guard<std::mutex> lock(m_updateMutex);
        result = m_updateResult;
    }

    switch (result.status)
    {
    case UpdateChecker::Result::Status::Failed:
        Logger::debug("Update check failed: " + result.detail);
        if (m_manualCheck)
        {
            std::string text = "Could not check for updates.\n\n" + result.detail;
            std::vector<wchar_t> wtext = Logger::utf16(text);
            MessageBoxW(m_window.handle(), wtext.data(),
                        L"Spotify Volume Hotkeys \u2014 Update Check",
                        MB_OK | MB_ICONWARNING | MB_TOPMOST);
        }
        break;

    case UpdateChecker::Result::Status::UpToDate:
        Logger::info("Update check: already on the latest version (v" +
                     std::string(Version::String) + ").");
        if (m_manualCheck)
        {
            std::string text = "You are running the latest version (v" +
                               std::string(Version::String) + ").";
            std::vector<wchar_t> wtext = Logger::utf16(text);
            MessageBoxW(m_window.handle(), wtext.data(),
                        L"Spotify Volume Hotkeys \u2014 Update Check",
                        MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        }
        break;

    case UpdateChecker::Result::Status::UpdateAvailable:
        m_updateAvailable = true;
        m_updateLatest = result.latestVersion;
        m_updateUrl = result.releaseUrl;
        m_installerUrl = result.installerUrl;
        m_installerSha256 = result.installerSha256;
        Logger::info("Update available: v" + m_updateLatest + " (running v" +
                     std::string(Version::String) + ").");
        if (!m_updateDismissed || m_manualCheck)
        {
            showUpdatePrompt();
        }
        break;
    }
}

void AppController::showUpdatePrompt()
{
    const bool selfInstall = canSelfInstall();

    std::string text = "A new version of Spotify Volume Hotkeys is available.\n\n"
                       "Installed: v" + std::string(Version::String) + "\n"
                       "Latest:    v" + m_updateLatest + "\n\n";
    text += selfInstall ? "Download and install it now?" : "Open the download page now?";

    std::vector<wchar_t> wtext = Logger::utf16(text);
    const int choice = MessageBoxW(m_window.handle(), wtext.data(),
                                   L"Spotify Volume Hotkeys \u2014 Update Available",
                                   MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST);
    if (choice == IDYES)
    {
        if (selfInstall)
        {
            startInstall();
        }
        else
        {
            openReleasePage();
        }
    }
    else
    {
        m_updateDismissed = true; // suppress only for this session
    }
}

bool AppController::canSelfInstall() const
{
    return !m_installerUrl.empty() && !m_installerSha256.empty() &&
           Updater::isManagedInstall();
}

void AppController::startInstall()
{
    if (m_installRunning.load())
    {
        return;
    }
    if (m_installThread.joinable())
    {
        m_installThread.join();
    }

    const std::string url = m_installerUrl;
    const std::string sha = m_installerSha256;
    if (url.empty() || sha.empty())
    {
        openReleasePage();
        return;
    }

    m_installRunning = true;
    Logger::info("Downloading update from " + url);

    m_installThread = std::thread([this, url, sha]()
    {
        struct RunningGuard
        {
            std::atomic<bool> &flag;
            ~RunningGuard() { flag = false; }
        } guard{m_installRunning};

        std::wstring installerPath;
        std::string error;
        bool ok = false;
        if (Updater::downloadInstaller(url, sha, installerPath, error))
        {
            Logger::info("Update downloaded and verified; launching installer.");
            ok = Updater::runInstaller(installerPath, error);
        }

        {
            std::lock_guard<std::mutex> lock(m_installMutex);
            m_installOk = ok;
            m_installError = error;
        }

        if (!m_updateStop.load())
        {
            PostMessageW(m_window.handle(), WM_APP_INSTALL_RESULT, 0, 0);
        }
    });
}

void AppController::onInstallResult()
{
    bool ok = false;
    std::string error;
    {
        std::lock_guard<std::mutex> lock(m_installMutex);
        ok = m_installOk;
        error = m_installError;
    }

    if (ok)
    {
        Logger::info("Installer launched; exiting so it can replace the running files.");
        m_exitAction = ExitAction::Exit;
        PostQuitMessage(0);
        return;
    }

    Logger::error("Update install failed: " + error);
    std::string text = "Could not install the update.\n\n" + error +
                       "\n\nYou can also download it from the release page.";
    std::vector<wchar_t> wtext = Logger::utf16(text);
    const int choice = MessageBoxW(m_window.handle(), wtext.data(),
                                   L"Spotify Volume Hotkeys \u2014 Update Failed",
                                   MB_YESNO | MB_ICONWARNING | MB_TOPMOST);
    if (choice == IDYES)
    {
        openReleasePage();
    }
}

void AppController::openReleasePage()
{
    if (m_updateUrl.empty())
    {
        return;
    }

    std::vector<wchar_t> url = Logger::utf16(m_updateUrl);
    ShellExecuteW(NULL, L"open", url.data(), NULL, NULL, SW_SHOWNORMAL);
}

std::wstring AppController::updateMenuLabel() const
{
    if (!m_updateAvailable)
    {
        return L"Check for Updates...";
    }
    return L"Update to v" + std::wstring(m_updateLatest.begin(), m_updateLatest.end()) + L"...";
}
