# AGENTS.md

Windows-only Win32 system-tray utility (C++20, MSVC, vcpkg). No test framework — verify by building and exercising the app/installer.

## Build
Toolchain is MSVC + vcpkg; the `windows-vs` preset targets the "Visual Studio 18 2026" generator. From WSL, drive Windows CMake through `cmd.exe`:
```
cmd.exe /c "cd /d C:\Users\steve\Documents\Code\spotifyVolumeHotkeys && cmake --preset windows-vs"
cmd.exe /c "cd /d C:\Users\steve\Documents\Code\spotifyVolumeHotkeys && cmake --build --preset release"
```
- Output: `build/windows-vs/Release/SpotifyVolumeHotkey.exe` (ships with the vcpkg `*.dll`s).
- `VCPKG_ROOT` must be set on the Windows side (manifest mode pulls cpp-httplib/OpenSSL + nlohmann-json).
- Build fails with `LNK1104` while `SpotifyVolumeHotkey.exe` is running — stop it first.
- Sources are globbed (`CONFIGURE_DEPENDS`); new files require a reconfigure before they compile.

## Versioning (single source of truth)
`vcpkg.json` `version-string` -> CMake `SVH_VERSION` -> `SVH_VERSION_STRING` define (`include/Version.hpp`) + generated `version.rc`. CI overrides with `-DSVH_VERSION=<tag>`. Bump only `vcpkg.json`; do not hardcode versions.
- The version resource must be `1 VERSIONINFO`, not `VS_VERSION_INFO VERSIONINFO` — the named form is invisible to Windows version APIs.
- A leftover `SVH_VERSION` can persist in the CMake cache; clear it with `cmake --preset windows-vs -U SVH_VERSION`.

## Updates & releases
- Update checks hit `.../releases/latest/download/version.json` (not the rate-limited REST API); the manifest carries `installer_url` + `sha256`.
- Test hook: set `SVH_UPDATE_BASE_URL` to point the check at a local server (the GitHub fallback is skipped). Serve the manifest at `<base>/releases/latest/download/version.json`.
- `Updater` self-installs only when `isManagedInstall()` matches the Inno uninstall key; otherwise it opens the release page.
- Releases are cut by pushing a `v*.*.*` tag; `.github/workflows/release.yml` builds, compiles the Inno installer (`choco install innosetup`, then `iscc`), generates `version.json`, and publishes installer + manifest (the zip was dropped).
- Inno `AppId` in `installer/SpotifyVolumeHotkeys.iss` must match the GUID in `src/core/Updater.cpp` `UNINSTALL_KEY` (`{GUID}_is1`).
- Installer is per-user (`%LocalAppData%\Programs\SpotifyVolumeHotkeys`) and unsigned (SmartScreen warning on manual install).

## Gotchas
- Single instance via mutex `Local\SpotifyVolumeHotkeys_SingleInstance`; a second launch exits silently.
- Autostart is an HKCU `Run` value holding the exe path; `Autostart::isEnabled()` compares it to the current exe and rewrites on mismatch (self-heal after updates/moves).
- Restart uses `ExitAction::Restart` to relaunch; updates use `ExitAction::Exit` and let the installer relaunch.

## Conventions
- Commit prefixes: `feat:`, `fix:`, `ci:`, and a standalone `release: version X.Y.Z` bump before tagging.
