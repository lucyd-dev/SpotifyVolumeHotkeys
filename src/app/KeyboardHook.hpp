#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <thread>
#include <vector>

// Experimental low-level keyboard hook (WH_KEYBOARD_LL).
//
// RegisterHotKey is disabled by the system while a game holds the keyboard in
// exclusive mode, which is why native volume keys stop working in some games.
// A low-level hook runs earlier in the input path, so it still sees (and can
// swallow) the keys.
//
// The hook is installed on its own dedicated thread which immediately returns
// from the callback, so keyboard input is never delayed by the application's
// own work (Spotify HTTP calls, etc). The callback only recognizes the keys
// handed to setKeys() and posts a message to the owner window; everything else
// is passed straight through.
class KeyboardHook
{
public:
    KeyboardHook() = default;
    ~KeyboardHook();

    KeyboardHook(const KeyboardHook &) = delete;
    KeyboardHook &operator=(const KeyboardHook &) = delete;

    void setKeys(const std::vector<UINT> &keys);
    bool start(HWND notifyWindow, UINT notifyMessage);
    void stop();
    bool isRunning() const noexcept { return m_running.load(); }

private:
    static constexpr int MAX_KEYS = 8;

    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam);
    void threadMain();

    HWND m_notifyWindow = nullptr;
    UINT m_notifyMessage = 0;

    std::atomic<UINT> m_keys[MAX_KEYS]{};
    std::atomic<int> m_keyCount{0};

    std::thread m_thread;
    std::atomic<DWORD> m_threadId{0};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_installed{false};
};
