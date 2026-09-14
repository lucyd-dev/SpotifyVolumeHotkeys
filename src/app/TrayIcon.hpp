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
        MenuToggleAutostart,
        MenuRestart,
        MenuExit,
    };

    TrayIcon();

    void configure(HWND hwnd, HINSTANCE hInstance, UINT callbackMsg);
    bool add();
    void remove();

    void setStatus(const std::wstring &status);
    int showPopup(const std::wstring &status, bool autostartEnabled);

private:
    HWND m_hwnd;
    HINSTANCE m_hInst;
    UINT m_callbackMsg;
    bool m_added;
    HMENU m_activeMenu;
};