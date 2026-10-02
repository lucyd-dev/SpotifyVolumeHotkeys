#include "ConsoleWizard.hpp"
#include "HotkeyMap.hpp"
#include "Autostart.hpp"
#include "../core/Logger.hpp"

#include <cstdio>
#include <iostream>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

namespace
{
    bool g_useColor = false;

    const char *RESET = "\x1b[0m";
    const char *BOLD = "\x1b[1m";
    const char *DIM = "\x1b[2m";
    const char *PURPLE = "\x1b[38;2;170;102;255m";
    const char *PURPLE_DARK = "\x1b[38;2;130;60;220m";
    const char *PURPLE_BG = "\x1b[48;2;76;30;140m";
    const char *WHITE = "\x1b[38;2;240;232;255m";
    const char *GREEN = "\x1b[38;2;88;214;141m";
    const char *RED = "\x1b[38;2;255;99;99m";
    const char *YELLOW = "\x1b[38;2;255;201;94m";

    const char *c(const char *code)
    {
        return g_useColor ? code : "";
    }

    void enableConsoleFormatting(unsigned long &previousMode, bool &modeChanged)
    {
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (out != NULL && out != INVALID_HANDLE_VALUE)
        {
            DWORD mode = 0;
            if (GetConsoleMode(out, &mode))
            {
                if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
                {
                    g_useColor = true;
                }
                else if (SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
                {
                    g_useColor = true;
                    previousMode = mode;
                    modeChanged = true;
                }
            }
        }
        SetConsoleOutputCP(CP_UTF8);
    }

    void redirectConsoleStreams()
    {
        FILE *stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONIN$", "r", stdin);
        std::cout.clear();
        std::cin.clear();
    }

    void rule()
    {
        std::cout << "  " << c(PURPLE_DARK);
        for (int i = 0; i < 46; ++i)
        {
            std::cout << "\xE2\x94\x80";
        }
        std::cout << c(RESET) << "\n";
    }

    void section(const char *title)
    {
        std::cout << "\n  " << c(PURPLE_BG) << c(WHITE) << c(BOLD)
                  << "  " << title << "  " << c(RESET) << "\n";
    }

    std::string readLine()
    {
        std::cin.sync();
        std::string line;
        std::getline(std::cin, line);
        return line;
    }

    std::string trim(const std::string &value)
    {
        const char *whitespace = " \t\r\n";
        std::size_t start = value.find_first_not_of(whitespace);
        if (start == std::string::npos)
        {
            return std::string();
        }
        std::size_t end = value.find_last_not_of(whitespace);
        return value.substr(start, end - start + 1);
    }

    std::string promptRequired(const char *label, const std::string &fallback)
    {
        while (true)
        {
            std::cout << "  " << c(PURPLE) << label << c(RESET);
            if (!fallback.empty())
            {
                std::cout << " " << c(DIM) << "[" << fallback << "]" << c(RESET);
            }
            std::cout << ": " << std::flush;

            std::string input = trim(readLine());
            if (input.empty())
            {
                input = fallback;
            }
            if (!input.empty())
            {
                return input;
            }
            std::cout << "  " << c(RED) << label
                      << " cannot be empty, please try again." << c(RESET) << "\n";
        }
    }

    std::string promptHotkey(const char *label, const std::string &fallback)
    {
        while (true)
        {
            std::cout << "  " << c(PURPLE) << label << c(RESET) << " "
                      << c(DIM) << "[" << fallback << "]" << c(RESET) << ": " << std::flush;

            std::string input = trim(readLine());
            if (input.empty())
            {
                input = fallback;
            }

            std::string reason;
            if (HotkeyMap::validate(input, reason))
            {
                return HotkeyMap::normalize(input);
            }
            std::cout << "  " << c(YELLOW) << reason << c(RESET) << "\n";
        }
    }

    bool promptYesNo(const char *label, bool defaultValue)
    {
        const char *options = defaultValue ? "Y/n" : "y/N";
        while (true)
        {
            std::cout << "  " << c(PURPLE) << label << c(RESET) << " "
                      << c(DIM) << "[" << options << "]" << c(RESET) << ": " << std::flush;

            std::string input = trim(readLine());
            if (input.empty())
            {
                return defaultValue;
            }
            if (input == "y" || input == "Y" || input == "yes" || input == "Yes" || input == "YES")
            {
                return true;
            }
            if (input == "n" || input == "N" || input == "no" || input == "No" || input == "NO")
            {
                return false;
            }
            std::cout << "  " << c(YELLOW) << "Please answer y or n." << c(RESET) << "\n";
        }
    }
}

bool ConsoleWizard::begin()
{
    if (GetConsoleWindow() == NULL)
    {
        if (!AllocConsole())
        {
            Logger::error("Failed to allocate a console for the setup wizard.");
            Logger::fatal("Run from a terminal or create config.json manually.");
            return false;
        }
        m_ownsConsole = true;
    }

    m_previousOutputCp = GetConsoleOutputCP();
    redirectConsoleStreams();
    enableConsoleFormatting(m_previousConsoleMode, m_consoleModeChanged);
    m_codePageChanged = (m_previousOutputCp != GetConsoleOutputCP());

    Logger::setShowDialogs(false);
    return true;
}

bool ConsoleWizard::complete(AppConfig &config, Config &configStore)
{
    AppConfig disk = configStore.load();
    disk.clientId = config.clientId;
    disk.clientSecret = config.clientSecret;
    disk.volumeDownKey = config.volumeDownKey;
    disk.volumeUpKey = config.volumeUpKey;
    disk.autostart = config.autostart;

    if (!configStore.save(disk))
    {
        Logger::error("Setup wizard failed to save the configuration file.");
        return false;
    }

    if (!Autostart::setEnabled(disk.autostart))
    {
        std::cout << "  " << c(YELLOW)
                  << "Could not update autostart; you can toggle it from the tray menu later."
                  << c(RESET) << "\n"
                  << std::flush;
    }

    std::cout << "\n  " << c(GREEN) << "\xE2\x9C\x93 " << c(RESET) << c(BOLD)
              << "You're all set!" << c(RESET) << c(DIM)
              << "  Moving to the system tray..." << c(RESET) << "\n\n"
              << std::flush;

    Logger::info("First-run setup completed and configuration saved.");
    return true;
}

void ConsoleWizard::collect(AppConfig &config, const std::string &redirectUri)
{
    std::cout << "\n";
    std::cout << "  " << c(PURPLE) << c(BOLD) << "Spotify Volume Hotkeys" << c(RESET)
              << "  " << c(DIM) << "first-run setup" << c(RESET) << "\n";
    rule();
    std::cout << "  Welcome! Let's connect this app to your Spotify account.\n";
    std::cout << "  " << c(DIM)
              << "Your credentials are only saved once you log in, so you can" << c(RESET) << "\n";
    std::cout << "  " << c(DIM) << "quit at any time before then." << c(RESET) << "\n";

    section("1. Create a Spotify app");
    const std::string dashboard = "https://developer.spotify.com/dashboard";
    std::cout << "  On the dashboard, do the following:\n";
    std::cout << "    " << c(DIM) << "1)" << c(RESET) << " Log in (or sign up), then click "
              << c(BOLD) << "Create app" << c(RESET) << ".\n";
    std::cout << "    " << c(DIM) << "2)" << c(RESET) << " Name it anything you like.\n";
    std::cout << "    " << c(DIM) << "3)" << c(RESET) << " Add this exact Redirect URI:\n";
    std::cout << "         " << c(PURPLE) << redirectUri << c(RESET) << "\n";
    std::cout << "    " << c(DIM) << "4)" << c(RESET) << " Save, then copy the " << c(BOLD)
              << "Client ID" << c(RESET) << " and " << c(BOLD) << "Client Secret" << c(RESET)
              << ".\n\n";
    std::cout << "  Dashboard: " << c(PURPLE) << dashboard << c(RESET) << "\n";
    if (promptYesNo("Open the dashboard in your browser now", true))
    {
        ShellExecuteA(NULL, "open", dashboard.c_str(), NULL, NULL, SW_SHOWNORMAL);
        std::cout << "  " << c(GREEN) << "Opened in your default browser." << c(RESET) << "\n";
    }

    section("2. Enter your credentials");
    std::cout << "  " << c(DIM)
              << "Copy the Client ID and Client Secret from your app page." << c(RESET) << "\n";
    config.clientId = promptRequired("Client ID", config.clientId);
    config.clientSecret = promptRequired("Client Secret", config.clientSecret);

    section("3. Choose your hotkeys");
    std::cout << "  " << c(DIM)
              << "Use A-Z, 0-9, F1-F24, Space, or an arrow key. Press Enter to keep a default."
              << c(RESET) << "\n";
    config.volumeDownKey = promptHotkey(
        "Volume-down key", config.volumeDownKey.empty() ? "F13" : config.volumeDownKey);
    config.volumeUpKey = promptHotkey(
        "Volume-up key", config.volumeUpKey.empty() ? "F14" : config.volumeUpKey);

    section("4. Startup");
    config.autostart = promptYesNo("Start automatically with Windows", true);

    std::cout << "\n  " << c(DIM)
              << "Next, your browser will open so you can log in to Spotify." << c(RESET) << "\n"
              << std::flush;
}

void ConsoleWizard::detach()
{
    if (m_detached)
    {
        return;
    }
    m_detached = true;

    Logger::setShowDialogs(true);

    if (!m_ownsConsole)
    {
        if (m_codePageChanged)
        {
            SetConsoleOutputCP(m_previousOutputCp);
            m_codePageChanged = false;
        }
        if (m_consoleModeChanged)
        {
            HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
            if (out != NULL && out != INVALID_HANDLE_VALUE)
            {
                SetConsoleMode(out, (DWORD)m_previousConsoleMode);
            }
            m_consoleModeChanged = false;
        }
        return;
    }

    std::cout.flush();

    FILE *stream = nullptr;
    freopen_s(&stream, "NUL", "r", stdin);
    freopen_s(&stream, "NUL", "w", stdout);
    freopen_s(&stream, "NUL", "w", stderr);
    FreeConsole();
}
