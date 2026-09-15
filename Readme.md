# Spotify Volume Hotkeys

A lightweight, background Windows system tray utility to control Spotify volume globally via custom hotkeys (defaulting to `F13` and `F14`).

## Features

- **Volume hotkeys from anywhere:** Press `F13` and `F14` (customizable) to lower and raise Spotify's volume while using any application.
- **Runs quietly in the background:** No windows, no terminal — just a small icon in your system tray.
- **System tray menu:** Left or Right-click the tray icon to see the current status, edit your settings, open the logs folder, toggle starting with Windows, restart, or exit.
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
