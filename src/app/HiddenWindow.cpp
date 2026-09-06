#include "HiddenWindow.hpp"
#include "AppController.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

bool HiddenWindow::create(HINSTANCE hInstance, AppController *owner)
{
    m_hwnd = CreateWindowExW(0, L"Static", NULL, 0, 0, 0, 0, 0,
                             NULL, NULL, hInstance, NULL);
    if (!m_hwnd)
    {
        return false;
    }
    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(owner));
    SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&HiddenWindow::wndProc));
    return true;
}

void HiddenWindow::destroy()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }
}

LRESULT CALLBACK HiddenWindow::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AppController *owner = reinterpret_cast<AppController *>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (owner && owner->handleMessage(hwnd, msg, wParam, lParam))
    {
        return TRUE;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}