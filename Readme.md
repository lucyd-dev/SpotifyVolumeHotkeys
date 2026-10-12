# <img src="assets/svh.ico" width="24" alt="Spotify Volume Hotkeys Icon"> Spotify Volume Hotkeys

A lightweight, background Windows system tray utility to control Spotify volume globally via custom hotkeys (defaulting to the keyboard's native `VolumeDown` and `VolumeUp` keys).

## Features

- **Volume hotkeys from anywhere:** Press the keyboard's native volume-down/up keys (customizable, defaults `VolumeDown`/`VolumeUp`) to lower and raise Spotify's volume while using any application.
- **Runs quietly in the background:** No windows, no terminal — just a small icon in your system tray.
- **System tray menu:** Left or Right-click the tray icon to see the current status, check for updates, edit your settings, open the logs, toggle starting with Windows, restart, or exit.
- **Self-updating:** On startup the app quietly checks GitHub for a newer release and, when one exists, offers to download and install it in place — both in a dialog and via the tray menu.
- **Start with Windows:** Optional autostart so your hotkeys are always ready after a reboot.
- **Edit settings without restarting:** Save changes to your config file and they take effect automatically.
- **First-run setup wizard:** Walking you through connecting to Spotify and setting your hotkeys.
- **Logs in one place:** All output goes to `%APPDATA%\SpotifyVolumeHotkeys\spotify_volume_hotkeys.log`; open it from **Open Logs...** in the tray menu.

> [!IMPORTANT]
> Experience is not buttery smooth as wanted, thats due to Spotify's API being heavily rate-limited. See the [Rate Limiting](#rate-limiting) section below.

---

## Installation

Download `SpotifyVolumeHotkeys-Setup-<version>.exe` from the
[latest release](https://github.com/lucyd-dev/SpotifyVolumeHotkeys/releases/latest)
and run it. It installs per-user to
`%LocalAppData%\Programs\SpotifyVolumeHotkeys` (no administrator rights needed) and
creates a Start Menu shortcut. Future updates are downloaded and installed in place
from within the app.

> [!NOTE]
> The installer is not code-signed, so Windows SmartScreen may show a
> "Windows protected your PC" prompt the first time you run it manually. Choose
> **More info** → **Run anyway**. Updates installed from inside the app are not
> affected.

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

You never have to touch a config file to get started. On the first launch a friendly
**setup wizard** opens in a console, guides you through connecting to Spotify, and then
gets out of the way, leaving the app running entirely in the system tray.

### First-time setup (guided wizard)

1. **Launch the app.** Double-click `SpotifyVolumeHotkey.exe`, or start it from a
   terminal if you prefer.
2. **Follow the wizard.** It walks you through each step and validates your answers, so
   you can just read the prompts and press Enter:
   - **Create a Spotify app:** before opening your browser, the wizard lists exactly what
     to do on the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard):
     create an app and paste the **Redirect URI** it shows on screen. It then offers to
     open the dashboard for you (`Open the dashboard in your browser now [Y/n]`).
   - **Enter your credentials:** paste the app's **Client ID** and **Client Secret**.
   - **Choose your hotkeys:** pick the volume-down and volume-up keys (defaults
     `VolumeDown` / `VolumeUp`, i.e. the keyboard's own volume keys). Valid keys are
     `A`-`Z`, `0`-`9`, `F1`-`F24`, `Space`, the arrow keys, and `VolumeUp`/`VolumeDown`/
     `VolumeMute`; press Enter to accept the suggested default.
   - **Choose startup behaviour:** decide whether the app should start automatically with
     Windows (default yes).
3. **Log in to Spotify.** A browser window opens; approve access. The wizard confirms
   success, the console closes, and the app docks into your system tray.

> [!NOTE]
> Your credentials are only saved after a successful login. If you close the wizard early
> or authentication fails, just launch the app again and it will ask for them.

### Everyday use (tray menu)

Once set up, there is no window to manage, only the tray icon:

- **Left- or right-click the tray icon** to open the menu.
- The top entry shows the current status, e.g. `Player active · 42%`, `No active device`,
  or `Rate limited`.
- From the menu you can **Check for Updates...** (or **Update to vX.Y.Z...** when a newer
  release is known, which downloads and installs it), **GitHub Repository...**,
  **Edit Config...**, **Open Logs...**, toggle **Start with Windows**, **Restart**,
  or **Exit**.
- Use your hotkeys from any application to change Spotify's volume.

### Using the native volume keys

With the default hotkeys (`VolumeDown` / `VolumeUp`), the app registers the keyboard's own
volume keys as global hotkeys. While the app is running, pressing them **adjusts Spotify's
volume and no longer changes the Windows master volume**. When no Spotify playback device is
active, the keys do nothing at all (the press is still consumed).

> [!NOTE]
> This relies on the keys reaching Windows as standard `VK_VOLUME_UP` / `VK_VOLUME_DOWN`
> events, which is true for the vast majority of keyboards and laptops. On some devices the
> keys are delivered as HID Consumer Control input handled below user mode, in which case the
> master volume may still change. If the master volume moves (or the app logs that the hotkey
> is "in use by another application"), pick a different key in `config.json` — for example
> `F13`/`F14` — via **Edit Config...**.

### Manual configuration

Prefer editing files? Choose **Edit Config...** from the tray menu, or open
`%APPDATA%\SpotifyVolumeHotkeys\config.json` directly:

```json
{
  "client_id": "YOUR_CLIENT_ID",
  "client_secret": "YOUR_CLIENT_SECRET",
  "refresh_token": "YOUR_SAVED_TOKEN",
  "autostart": true,
  "hotkeys": {
    "volume_down": "VolumeDown",
    "volume_up": "VolumeUp"
  },
  "pollingIntervals": {
    "input": 250,
    "player": 5000
  }
}
```

While the app is running, saving changes to `config.json` (credentials, hotkeys,
autostart, polling intervals) is detected automatically and applied without a restart.

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
