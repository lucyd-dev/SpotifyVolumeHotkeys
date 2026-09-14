#include "Autostart.hpp"
#include "../core/Logger.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

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
}

namespace Autostart
{
    bool isEnabled()
    {
        DWORD type = 0;
        DWORD size = 0;
        LONG res = RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE,
                                RRF_RT_REG_SZ, &type, NULL, &size);
        return res == ERROR_SUCCESS && size > 0;
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