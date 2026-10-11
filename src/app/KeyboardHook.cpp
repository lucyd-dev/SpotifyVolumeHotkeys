#include "KeyboardHook.hpp"

namespace
{
    // Only one hook is ever active; the callback has no user-pointer, so the
    // instance is reached through this file-local pointer.
    KeyboardHook *g_instance = nullptr;
}

KeyboardHook::~KeyboardHook()
{
    stop();
}

void KeyboardHook::setKeys(const std::vector<UINT> &keys)
{
    int count = static_cast<int>(keys.size());
    if (count > MAX_KEYS)
    {
        count = MAX_KEYS;
    }
    for (int i = 0; i < count; ++i)
    {
        m_keys[i].store(keys[static_cast<std::size_t>(i)], std::memory_order_relaxed);
    }
    m_keyCount.store(count, std::memory_order_relaxed);
}

bool KeyboardHook::start(HWND notifyWindow, UINT notifyMessage)
{
    if (m_thread.joinable())
    {
        stop();
    }

    m_notifyWindow = notifyWindow;
    m_notifyMessage = notifyMessage;
    m_installed.store(false);
    m_running.store(true);
    g_instance = this;

    m_thread = std::thread(&KeyboardHook::threadMain, this);

    for (int i = 0; i < 200 && !m_installed.load() && m_running.load(); ++i)
    {
        Sleep(2);
    }

    return m_installed.load();
}

void KeyboardHook::stop()
{
    if (m_running.exchange(false))
    {
        DWORD tid = m_threadId.load();
        if (tid)
        {
            PostThreadMessageW(tid, WM_QUIT, 0, 0);
        }
    }

    if (m_thread.joinable())
    {
        m_thread.join();
    }

    m_threadId.store(0);
    g_instance = nullptr;
    m_installed.store(false);
}

void KeyboardHook::threadMain()
{
    m_threadId.store(GetCurrentThreadId());

    // Force the thread message queue into existence before installing the hook.
    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, &KeyboardHook::hookProc,
                                   GetModuleHandleW(nullptr), 0);
    if (!hook)
    {
        m_installed.store(false);
        m_running.store(false);
        return;
    }
    m_installed.store(true);

    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(hook);
    m_installed.store(false);
}

LRESULT CALLBACK KeyboardHook::hookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        const KBDLLHOOKSTRUCT *info = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);
        const bool down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        const bool up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        if ((down || up) && !(info->flags & LLKHF_INJECTED))
        {
            KeyboardHook *self = g_instance;
            if (self)
            {
                const UINT vk = static_cast<UINT>(info->vkCode);
                const int count = self->m_keyCount.load(std::memory_order_relaxed);
                for (int i = 0; i < count; ++i)
                {
                    if (self->m_keys[i].load(std::memory_order_relaxed) == vk)
                    {
                        if (down && self->m_notifyWindow)
                        {
                            PostMessageW(self->m_notifyWindow, self->m_notifyMessage,
                                         static_cast<WPARAM>(vk), 0);
                        }
                        // Swallow both keydown and keyup so the game and the
                        // Windows master volume never see the key.
                        return 1;
                    }
                }
            }
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
