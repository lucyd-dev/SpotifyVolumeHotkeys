# Spotify Volume Hotkeys

A lightweight, background Windows system tray utility to control Spotify volume globally via custom hotkeys (defaulting to `F13` and `F14`).

## Features

- **Invisible Background Process:** Runs as a native Windows GUI app (`WinMain`) with no persistent command prompt or terminal window.
- **System Tray Management:** Right-click context menu to view status, edit settings, open logs, toggle Windows autostart, or exit.
- **First-Run Setup Wizard:** Automatically spawns a temporary terminal on initial launch if credentials are missing to guide setup interactively.
- **Live Config Watcher:** Automatically detects modifications to `%APPDATA%\SpotifyVolumeHotkeys\config.json` and updates hotkeys or authentication in real time.
- **Autostart Integration:** Clean, non-intrusive autostart via `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.

---

## Prerequisites

- **OS:** Windows 10 or Windows 11 (64-bit)
- **Compiler:** MSVC with C++20 support (Visual Studio 2022 / 2026 or Build Tools)
- **Build System:** [CMake](https://cmake.org/download/) (v3.20+)
- **Package Manager:** [vcpkg](https://github.com/microsoft/vcpkg)

---

## Building from Source

This project uses **vcpkg Manifest Mode** (`vcpkg.json`). Dependencies (`cpp-httplib`, `nlohmann-json`) are automatically fetched and built during CMake configuration.

1. **Clone the repository:**
   ```powershell
   git clone [https://github.com/](https://github.com/)<YourUsername>/SpotifyVolumeHotkeys.git
   cd SpotifyVolumeHotkeys
   ```

2. **Configure CMake:**
   Set the `CMAKE_TOOLCHAIN_FILE` to your local vcpkg toolchain path:
   ```powershell
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
   ```

3. **Build Release Binary:**
   ```powershell
   cmake --build build --config Release
   ```

The compiled binary will be located at `build/Release/spotifyVolumeHotkeys.exe`.

---

## Setup & Configuration

1. Create an webAPI application in the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard).
2. Set the Redirect URI to `http://localhost:8888/callback`.
3. Launch `spotifyVolumeHotkeys.exe`.
4. On first launch, a console wizard will ask for your **Client ID**, **Client Secret**, and desired volume keys (defaults to `F13` / `F14`).
5. After authentication, the console closes and the app docks quietly into your Windows System Tray.

### Manual Configuration
You can edit configuration at any time by selecting **Edit Config & Hotkeys** from the tray icon or by opening:
`%APPDATA%\SpotifyVolumeHotkeys\config.json`

```json
{
  "client_id": "YOUR_CLIENT_ID",
  "client_secret": "YOUR_CLIENT_SECRET",
  "refresh_token": "YOUR_SAVED_TOKEN",
  "autostart": true,
  "hotkeys": {
    "volume_down": "F13",
    "volume_up": "F14"
  }
}
```

---

## License

This project is licensed under the MIT License.
