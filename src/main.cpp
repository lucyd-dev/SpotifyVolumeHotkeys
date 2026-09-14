#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <string>

#include "core/Logger.hpp"
#include "core/HttpClient.hpp"
#include "core/Config.hpp"
#include "auth/Auth.hpp"
#include "app/AppController.hpp"

const std::string REDIRECT_URI = "http://127.0.0.1:8888/callback";

int wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    HANDLE instanceMutex = CreateMutexW(NULL, FALSE,
                                        L"Local\\SpotifyVolumeHotkeys_SingleInstance");
    if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(NULL, L"Another instance is already running in the tray.",
                    L"SpotifyVolumeHotkeys", MB_OK | MB_ICONINFORMATION);
        CloseHandle(instanceMutex);
        return 0;
    }

    try
    {
        Config config;
        std::filesystem::create_directories(config.path().parent_path());

        std::filesystem::path logPath = config.path().parent_path() / "spotify_volume_hotkeys.log";
        Logger::setLogFile(logPath.string());
        Logger::cleanupLogFile();

        if (!HttpClient::init())
        {
            Logger::fatal("HttpClient initialization failed.");
            return 1;
        }

        AppConfig appConfig = config.load();

        Logger::info("Initializing authentication...");
        Auth auth(REDIRECT_URI);
        if (!auth.authenticate())
        {
            Logger::fatal("Auth failed.");
            return 1;
        }
        Logger::info("Authentication successful!");

        AppController controller(appConfig, auth);
        if (!controller.startup(hInstance))
        {
            Logger::fatal("Failed to start the application.");
            return 1;
        }

        Logger::info("App started. Wait for input...");
        AppController::ExitAction action = controller.run();
        controller.shutdown();

        if (action == AppController::ExitAction::Restart)
        {
            if (instanceMutex)
            {
                CloseHandle(instanceMutex);
                instanceMutex = NULL;
            }

            wchar_t exePath[MAX_PATH];
            DWORD exeLen = GetModuleFileNameW(NULL, exePath, MAX_PATH);
            if (exeLen > 0 && exeLen < MAX_PATH)
            {
                exePath[exeLen] = L'\0';
                ShellExecuteW(NULL, L"open", exePath, L"--restart", NULL, SW_SHOWNORMAL);
            }
        }
        return 0;
    }
    catch (const std::exception &e)
    {
        Logger::fatal(std::string("Unhandled exception: ") + e.what());
        return 1;
    }
}