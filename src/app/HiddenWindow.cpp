#include "HiddenWindow.hpp"
#include "AppController.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

bool HiddenWindow::create(HINSTANCE hInstance, AppController *owner)
{
    HWND hwnd = CreateWindowExW(0, L"Static", NULL, 0, 0, 0, 0, 0,
                                NULL, NULL, hInstance, NULL);
    if (!hwnd)
    {
        return false;
    }

    WindowData *data = new WindowData();
    data->owner = owner;
    data->originalProc = reinterpret_cast<WNDPROC>(
        GetWindowLongPtrW(hwnd, GWLP_WNDPROC));

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                      reinterpret_cast<LONG_PTR>(&HiddenWindow::wndProc));
    m_hwnd = hwnd;
    return true;
}

void HiddenWindow::destroy()
{
    if (m_hwnd)
    {
        WindowData *data = reinterpret_cast<WindowData *>(
            GetWindowLongPtrW(m_hwnd, GWLP_USERDATA));
        DestroyWindow(m_hwnd);
        delete data;
        m_hwnd = NULL;
    }
}

LRESULT CALLBACK HiddenWindow::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WindowData *data = reinterpret_cast<WindowData *>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (data && data->owner &&
        data->owner->handleMessage(hwnd, msg, wParam, lParam))
    {
        return TRUE;
    }

    WNDPROC originalProc = data ? data->originalProc : NULL;
    if (originalProc)
    {
        return CallWindowProcW(originalProc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}