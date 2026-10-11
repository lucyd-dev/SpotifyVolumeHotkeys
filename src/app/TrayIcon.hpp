#pragma once

#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class TrayIcon
{
public:
    enum MenuCommand
    {
        MenuStatus = 0,
        MenuEditConfig,
        MenuOpenLogs,
        MenuOpenGitHub,
        MenuToggleAutostart,
        MenuCheckUpdate,
        MenuRestart,
        MenuExit,
    };

    struct MenuState
    {
        std::wstring status;
        bool autostartEnabled = false;
        bool updateAvailable = false;
        std::wstring updateLabel; // e.g. L"Update to v0.2.4..."
    };

    TrayIcon();

    void configure(HWND hwnd, HINSTANCE hInstance, UINT callbackMsg);
    bool add();
    void remove();

    void setStatus(const std::wstring &status);
    int showPopup(const MenuState &state);

private:
    HWND m_hwnd;
    HINSTANCE m_hInst;
    UINT m_callbackMsg;
    bool m_added;
    HMENU m_activeMenu;
};