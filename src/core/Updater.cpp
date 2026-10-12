#include "Updater.hpp"
#include "HttpClient.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <openssl/evp.h>

#include <cctype>
#include <fstream>
#include <string>

namespace
{
    // Matches AppId in installer/SpotifyVolumeHotkeys.iss. Inno appends "_is1".
    const wchar_t *UNINSTALL_KEY =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
        L"{B7D2E9C4-3F1A-4E7B-9C2D-5A6F8E0B1C3D}_is1";

    std::wstring currentExeDir()
    {
        wchar_t buf[MAX_PATH];
        const DWORD len = GetModuleFileNameW(NULL, buf, MAX_PATH);
        if (len == 0 || len >= MAX_PATH)
        {
            return std::wstring();
        }
        std::wstring path(buf, len);
        const std::size_t slash = path.find_last_of(L"\\/");
        return slash == std::wstring::npos ? path : path.substr(0, slash);
    }

    std::wstring trimTrailingSlash(std::wstring value)
    {
        while (!value.empty() && (value.back() == L'\\' || value.back() == L'/'))
        {
            value.pop_back();
        }
        return value;
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

    bool equalsIgnoreCaseAscii(const std::string &a, const std::string &b)
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
            {
                return false;
            }
        }
        return true;
    }

    std::string sha256Hex(const std::string &data)
    {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digestLen = 0;

        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        if (!ctx)
        {
            return std::string();
        }
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(ctx, data.data(), data.size());
        EVP_DigestFinal_ex(ctx, digest, &digestLen);
        EVP_MD_CTX_free(ctx);

        static const char *hex = "0123456789abcdef";
        std::string out;
        out.reserve(digestLen * 2);
        for (unsigned int i = 0; i < digestLen; ++i)
        {
            out.push_back(hex[digest[i] >> 4]);
            out.push_back(hex[digest[i] & 0x0F]);
        }
        return out;
    }
}

namespace Updater
{
    bool isManagedInstall()
    {
        const std::wstring exeDir = trimTrailingSlash(currentExeDir());
        if (exeDir.empty())
        {
            return false;
        }

        DWORD type = 0;
        DWORD size = 0;
        if (RegGetValueW(HKEY_CURRENT_USER, UNINSTALL_KEY, L"InstallLocation",
                         RRF_RT_REG_SZ, &type, nullptr, &size) != ERROR_SUCCESS ||
            size == 0)
        {
            return false;
        }

        std::wstring installLocation(size / sizeof(wchar_t), L'\0');
        if (RegGetValueW(HKEY_CURRENT_USER, UNINSTALL_KEY, L"InstallLocation",
                         RRF_RT_REG_SZ, &type, installLocation.data(), &size) != ERROR_SUCCESS)
        {
            return false;
        }
        while (!installLocation.empty() && installLocation.back() == L'\0')
        {
            installLocation.pop_back();
        }

        return equalsIgnoreCase(trimTrailingSlash(installLocation), exeDir);
    }

    bool downloadInstaller(const std::string &url,
                           const std::string &expectedSha256,
                           std::wstring &outPath,
                           std::string &error)
    {
        std::string body;
        std::string httpError;
        if (!HttpClient::getFollowingRedirects(url, body, httpError))
        {
            error = "download failed: " + httpError;
            return false;
        }
        if (body.empty())
        {
            error = "downloaded installer is empty";
            return false;
        }

        const std::string actual = sha256Hex(body);
        if (actual.empty() || !equalsIgnoreCaseAscii(actual, expectedSha256))
        {
            error = "SHA-256 mismatch (expected " + expectedSha256 + ", got " + actual + ")";
            return false;
        }

        wchar_t tempDir[MAX_PATH];
        const DWORD len = GetTempPathW(MAX_PATH, tempDir);
        if (len == 0 || len >= MAX_PATH)
        {
            error = "could not resolve the temp directory";
            return false;
        }
        const std::wstring path = std::wstring(tempDir) + L"SpotifyVolumeHotkeys-Setup.exe";

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            error = "could not open a temporary file for writing";
            return false;
        }
        out.write(body.data(), static_cast<std::streamsize>(body.size()));
        out.close();
        if (!out)
        {
            DeleteFileW(path.c_str());
            error = "failed to write the installer to disk";
            return false;
        }

        outPath = path;
        return true;
    }

    bool runInstaller(const std::wstring &installerPath, std::string &error)
    {
        const std::wstring params =
            L"/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /NOCANCEL /CLOSEAPPLICATIONS";

        SHELLEXECUTEINFOW info = {};
        info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
        info.lpVerb = L"open";
        info.lpFile = installerPath.c_str();
        info.lpParameters = params.c_str();
        info.nShow = SW_SHOWNORMAL;

        if (!ShellExecuteExW(&info))
        {
            error = "ShellExecuteEx failed (error " + std::to_string(GetLastError()) + ")";
            return false;
        }
        return true;
    }
}
