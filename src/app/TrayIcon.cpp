#include "TrayIcon.hpp"
#include "resource.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <cstdint>

namespace
{
    typedef int(WINAPI *PFNSetPreferredAppMode)(DWORD);
    typedef int(WINAPI *PFNFlushMenuThemes)();

    void applyDarkMode()
    {
        HMODULE uxtheme = LoadLibraryW(L"uxtheme.dll");
        if (!uxtheme)
        {
            return;
        }

        PFNSetPreferredAppMode setPreferredAppMode =
            reinterpret_cast<PFNSetPreferredAppMode>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
        PFNFlushMenuThemes flushMenuThemes =
            reinterpret_cast<PFNFlushMenuThemes>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));

        if (setPreferredAppMode)
        {
            setPreferredAppMode(1); // AllowDark
        }
        if (flushMenuThemes)
        {
            flushMenuThemes();
        }
    }
}

TrayIcon::TrayIcon()
    : m_hwnd(NULL),
      m_hInst(NULL),
      m_callbackMsg(0),
      m_added(false),
      m_activeMenu(NULL)
{
}

void TrayIcon::configure(HWND hwnd, HINSTANCE hInstance, UINT callbackMsg)
{
    applyDarkMode();
    m_hwnd = hwnd;
    m_hInst = hInstance;
    m_callbackMsg = callbackMsg;
}

bool TrayIcon::add()
{
    if (m_added || !m_hwnd)
    {
        return m_added;
    }

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = m_callbackMsg;
    nid.hIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(IDI_APPICON));
    if (!nid.hIcon)
    {
        nid.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(IDI_APPLICATION));
    }
    wcscpy_s(nid.szTip, L"SpotifyVolumeHotkeys");

    m_added = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
    if (m_added)
    {
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
    }
    return m_added;
}

void TrayIcon::remove()
{
    if (!m_added || !m_hwnd)
    {
        m_added = false;
        return;
    }

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    m_added = false;
}

void TrayIcon::setStatus(const std::wstring &status)
{
    if (!m_added || !m_hwnd)
    {
        return;
    }

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_TIP;
    wcsncpy_s(nid.szTip, status.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);

    if (m_activeMenu)
    {
        MENUITEMINFOW mii = {};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_TYPE;
        mii.fType = MFT_STRING;
        mii.dwTypeData = const_cast<wchar_t *>(status.c_str());
        mii.cch = static_cast<UINT>(status.size());
        SetMenuItemInfoW(m_activeMenu, (UINT)MenuStatus, FALSE, &mii);
    }
}

int TrayIcon::showPopup(const std::wstring &status, bool autostartEnabled)
{
    if (!m_hwnd)
    {
        return 0;
    }

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | MF_GRAYED, (UINT_PTR)MenuStatus, status.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, (UINT_PTR)MenuEditConfig, L"Edit Config...");
    AppendMenuW(menu, MF_STRING, (UINT_PTR)MenuOpenLogs, L"Open Logs...");
    AppendMenuW(menu, MF_STRING | (autostartEnabled ? MF_CHECKED : 0),
                (UINT_PTR)MenuToggleAutostart, L"&Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, (UINT_PTR)MenuRestart, L"Restart");
    AppendMenuW(menu, MF_STRING, (UINT_PTR)MenuExit, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);
    m_activeMenu = menu;
    int cmd = (int)TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
                                  pt.x, pt.y, 0, m_hwnd, NULL);
    m_activeMenu = NULL;
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
    return cmd;
}
