#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class AppController;

class HiddenWindow
{
public:
    bool create(HINSTANCE hInstance, AppController *owner);
    HWND handle() const noexcept { return m_hwnd; }
    void destroy();

private:
    struct WindowData
    {
        AppController *owner;
        WNDPROC originalProc;
    };

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd;
};