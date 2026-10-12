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
#include "app/ConsoleWizard.hpp"

const std::string REDIRECT_URI = "http://127.0.0.1:8888/callback";

namespace
{
    AppOptions parseOptions()
    {
        AppOptions options;

        int argc = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv)
        {
            return options;
        }

        for (int i = 1; i < argc; ++i)
        {
            const std::wstring arg = argv[i];
            if (arg == L"--check-update")
            {
                options.forceCheck = true;
            }
            else if (arg.rfind(L"--simulate-update", 0) == 0)
            {
                options.simulateUpdateVersion = "9.9.9";
                const std::size_t eq = arg.find(L'=');
                if (eq != std::wstring::npos && eq + 1 < arg.size())
                {
                    const std::wstring value = arg.substr(eq + 1);
                    std::string narrow;
                    narrow.reserve(value.size());
                    for (wchar_t c : value)
                    {
                        narrow.push_back(static_cast<char>(c));
                    }
                    options.simulateUpdateVersion = narrow;
                }
            }
        }

        LocalFree(argv);
        return options;
    }
}

int wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    const AppOptions options = parseOptions();

    HANDLE instanceMutex = CreateMutexW(NULL, FALSE,
                                        L"Local\\SpotifyVolumeHotkeys_SingleInstance");
    if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        // Already running in the tray; leave silently.
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

        AppConfig appConfig = config.load();

        ConsoleWizard wizard;
        struct WizardGuard
        {
            ConsoleWizard &wizard;
            ~WizardGuard() { wizard.detach(); }
        } wizardGuard{wizard};

        const bool needsWizard = appConfig.clientId.empty() || appConfig.clientSecret.empty();
        if (needsWizard)
        {
            if (!wizard.begin())
            {
                return 1;
            }
        }

        if (!HttpClient::init())
        {
            Logger::setShowDialogs(true);
            Logger::fatal("HttpClient initialization failed.");
            wizard.detach();
            return 1;
        }

        if (needsWizard)
        {
            wizard.collect(appConfig, REDIRECT_URI);
        }

        Logger::info("Initializing authentication...");
        Auth auth(REDIRECT_URI);
        if (needsWizard)
        {
            auth.applyConfig(appConfig);
        }
        if (!auth.authenticate())
        {
            Logger::setShowDialogs(true);
            Logger::fatal("Auth failed.");
            wizard.detach();
            return 1;
        }
        Logger::info("Authentication successful!");

        if (needsWizard && !wizard.complete(appConfig, config))
        {
            Logger::setShowDialogs(true);
            Logger::fatal("Failed to save the configuration file.");
            wizard.detach();
            return 1;
        }

        wizard.detach();

        AppController controller(appConfig, auth, options);
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