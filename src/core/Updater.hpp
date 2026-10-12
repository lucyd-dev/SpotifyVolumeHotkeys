#pragma once

#include <string>

namespace Updater
{
    // True when the running executable lives in the directory recorded by our
    // Inno Setup "InstallLocation", i.e. the app was installed rather than run
    // portably. Only managed installs are allowed to self-update.
    bool isManagedInstall();

    // Downloads the installer from `url`, verifies its SHA-256 against
    // `expectedSha256`, and writes it to a temporary file. On any failure the
    // partial file is removed and `error` is populated.
    bool downloadInstaller(const std::string &url,
                           const std::string &expectedSha256,
                           std::wstring &outPath,
                           std::string &error);

    // Launches the downloaded installer silently and detached so it can replace
    // this running executable (then relaunch it via its [Run] entry).
    bool runInstaller(const std::wstring &installerPath, std::string &error);
}
