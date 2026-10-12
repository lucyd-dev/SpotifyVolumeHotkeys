#include "Autostart.hpp"
#include "../core/Logger.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cwctype>
#include <string>

namespace
{
    const wchar_t *RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t *RUN_VALUE = L"SpotifyVolumeHotkeys";

    std::wstring commandLine()
    {
        wchar_t buf[MAX_PATH];
        DWORD len = GetModuleFileNameW(NULL, buf, MAX_PATH);
        if (len == 0 || len >= MAX_PATH)
        {
            return std::wstring();
        }
        buf[len] = L'\0';
        return std::wstring(buf);
    }

    std::wstring readStoredValue()
    {
        DWORD type = 0;
        DWORD size = 0;
        if (RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE, RRF_RT_REG_SZ,
                         &type, nullptr, &size) != ERROR_SUCCESS || size == 0)
        {
            return std::wstring();
        }

        std::wstring buffer(size / sizeof(wchar_t), L'\0');
        if (RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE, RRF_RT_REG_SZ,
                         &type, buffer.data(), &size) != ERROR_SUCCESS)
        {
            return std::wstring();
        }
        while (!buffer.empty() && buffer.back() == L'\0')
        {
            buffer.pop_back();
        }
        return buffer;
    }

    bool equalsIgnoreCase(const std::wstring &a, const std::wstring &b)
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            if (towlower(a[i]) != towlower(b[i]))
            {
                return false;
            }
        }
        return true;
    }
}

namespace Autostart
{
    bool isEnabled()
    {
        // Treat the entry as enabled only when it points at the current binary.
        // A moved/renamed install then self-heals via setEnabled(true) instead
        // of leaving a stale autostart path behind.
        const std::wstring stored = readStoredValue();
        if (stored.empty())
        {
            return false;
        }
        const std::wstring expected = L"\"" + commandLine() + L"\"";
        return equalsIgnoreCase(stored, expected);
    }

    bool setEnabled(bool enabled)
    {
        HKEY hk = NULL;
        LONG res = RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0,
                                 KEY_SET_VALUE | KEY_QUERY_VALUE, &hk);
        if (res != ERROR_SUCCESS)
        {
            Logger::error("Failed to open the Run registry key.");
            return false;
        }

        if (enabled)
        {
            std::wstring cmd = L"\"" + commandLine() + L"\"";
            res = RegSetValueExW(hk, RUN_VALUE, 0, REG_SZ,
                                 reinterpret_cast<const BYTE *>(cmd.c_str()),
                                 static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
        }
        else
        {
            res = RegDeleteValueW(hk, RUN_VALUE);
            if (res == ERROR_FILE_NOT_FOUND)
            {
                res = ERROR_SUCCESS;
            }
        }

        RegCloseKey(hk);

        if (res != ERROR_SUCCESS)
        {
            Logger::error("Failed to update the autostart registry value.");
            return false;
        }
        return true;
    }
}