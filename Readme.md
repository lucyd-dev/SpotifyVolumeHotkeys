# Spotify Volume Hotkeys

A lightweight, background Windows system tray utility to control Spotify volume globally via custom hotkeys (defaulting to `F13` and `F14`).

## Features

- **Volume hotkeys from anywhere:** Press `F13` and `F14` (customizable) to lower and raise Spotify's volume while using any application.
- **Runs quietly in the background:** No windows, no terminal — just a small icon in your system tray.
- **System tray menu:** Left or Right-click the tray icon to see the current status, edit your settings, open the logs, toggle starting with Windows, restart, or exit.
- **Start with Windows:** Optional autostart so your hotkeys are always ready after a reboot.
- **Edit settings without restarting:** Save changes to your config file and they take effect automatically.
- **First-run setup wizard:** Walking you through connecting to Spotify and setting your hotkeys.

> [!NOTE]
> The first-run setup wizard is still a work in progress.

> [!IMPORTANT]
> Experience is not buttery smooth as wanted, thats due to Spotify's API being heavily rate-limited. See the [Rate Limiting](#rate-limiting) section below.

---

## Prerequisites

- **OS:** Windows 10 or Windows 11 (64-bit)
- **Compiler:** MSVC with C++20 support (Windows Build Tools 2026)
- **Build System:** [CMake](https://cmake.org/download/) (v3.20+)
- **Package Manager:** [vcpkg](https://github.com/microsoft/vcpkg)

---

## Building from Source

This project uses **CMake Presets** and **vcpkg Manifest Mode** (`vcpkg.json`). Dependencies (`cpp-httplib`, `nlohmann-json`) are automatically fetched and built during CMake configuration. The preset-based setup is also used by the CI release workflow, so local and remote builds stay in sync.

### Prerequisites

- Set the `VCPKG_ROOT` environment variable to your local [vcpkg](https://github.com/microsoft/vcpkg) checkout (e.g. `C:\vcpkg`).
  In VS Code you can instead add it to your local `.vscode/settings.json`:
  ```json
  {
    "cmake.environment": {
      "VCPKG_ROOT": "C:/vcpkg"
    }
  }
  ```

### Visual Studio Code (recommended)

1. Open the repository in VS Code with the [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension installed.
2. Run **CMake: Select Configure Preset** (`Ctrl+Shift+P`) and choose `Windows (MSVC Visual Studio)`.
3. Press `Ctrl+Shift+B` or run **CMake: Build**.
4. To switch between Debug and Release, click the build variant label (e.g. `Debug`) in the VS Code status bar and pick `Release`, then press `Ctrl+Shift+B` again.

### Command line

```powershell
cmake --preset windows-vs
cmake --build --preset release
```

The compiled binary will be located at `build/windows-vs/Release/SpotifyVolumeHotkey.exe`.

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
  },
  "pollingIntervals": {
    "input": 250,
    "player": 5000
  }
}
```

---

## Rate Limiting

Spotify's Web API is heavily rate-limited, which makes smooth (continuous) volume changes impossible. This app works around it by:

- **Deferring volume changes** by a quarter of a second (`pollingIntervals.input`, default `250` ms) so rapid hotkey presses are batched into a single call.
- **Polling player data** (active device, current volume) only every 5 seconds (`pollingIntervals.player`, default `5000` ms).

Both intervals are configurable via `pollingIntervals` in `config.json` and are hot-reloaded when you save the file. Lowering them makes volume changes feel more responsive, but **at your own risk**: Spotify may answer with HTTP `429 Too Many Requests`, which stops all API calls until Spotify's `Retry-After` period has passed.

To check whether you are being rate-limited, watch the log file (`%APPDATA%\SpotifyVolumeHotkeys\spotify_volume_hotkeys.log`, or use **Open Logs** from the tray menu) for `WARN` messages starting with:

```
429 Too Many Requests from ...
```

If you see these, raise the intervals again (or restore the defaults) to avoid long cool-down periods.

---

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details.

`Made with 💜 by Lucyd since 2026`
