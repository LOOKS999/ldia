# Lida

A lightweight Windows tray app that follows your system light/dark theme and automatically
swaps the **wallpaper** and the **cursor scheme** when the theme changes.

Lida runs in the notification area, needs no installer, and keeps everything in one
`config.json` next to the executable.

---

## Features

- Detects Windows light/dark mode via
  `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme`
- Applies a wallpaper + cursor scheme per mode (Light / Dark)
- Polls the theme at a configurable interval (default `1000` ms)
- Tray menu for changing everything without editing files by hand
- Custom embedded app icon (multi-size `.ico`, also used for the taskbar/Alt-Tab)
- Portable: config lives beside `Lida.exe`

---

## Requirements

- Windows 10 or Windows 11
- Cursor schemes must already be installed (Windows **Mouse Properties → Pointers**),
  because Lida selects schemes by name; it does not install cursor files.

---

## Usage

1. Run `Lida.exe`. It starts minimized in the system tray.
2. Right-click (or left-click) the tray icon:

| Menu item | What it does |
|---|---|
| **Light Mode → Set Wallpaper...** | Pick the wallpaper used in light mode |
| **Light Mode → Set Cursor Scheme by Name...** | Type an installed scheme name, e.g. `Modest Cursors Light` |
| **Dark Mode → ...** | Same two options for dark mode |
| **Auto Switch on Theme Change** | Toggle automatic switching |
| **Apply Current Theme Now** | Re-apply the current mode immediately |
| **Restore Default Cursors** | Revert to the Windows default cursor scheme |
| **Reload Config** | Re-read `config.json` after manual edits |
| **Open Config Folder** | Open the folder containing `Lida.exe` |
| **Exit** | Quit |

The exact scheme name must match a value under
`HKCU\Control Panel\Cursors\Schemes`. You can list the installed names with:

```
reg query "HKCU\Control Panel\Cursors\Schemes"
```

---

## Configuration

`config.json` is created next to `Lida.exe` on first run:

```json
{
  "check_interval_ms": 1000,
  "auto_switch": true,
  "light": {
    "wallpaper": "C:\\Users\\You\\Pictures\\light.jpg",
    "cursorScheme": "Modest Cursors Light"
  },
  "dark": {
    "wallpaper": "C:\\Users\\You\\Pictures\\dark.jpg",
    "cursorScheme": "Modest Cursors Dark"
  }
}
```

Notes:

- `check_interval_ms` — how often the theme is checked. Lower = faster reaction,
  slightly more CPU. `1000` is a good default.
- `auto_switch` — set to `false` to disable automatic switching.
- Wallpaper paths may be absolute or relative to the folder containing `Lida.exe`.
  The tray UI stores them relative when the file lives inside that folder.
- Restart Lida (or use **Reload Config**) after editing the file manually.

---

## How the cursors are applied

Lida reads the selected scheme's value from
`HKCU\Control Panel\Cursors\Schemes`, then writes each of the 17 cursor roles
(Arrow, Help, AppStarting, Wait, Crosshair, IBeam, NWPen, No, SizeNS, SizeWE,
SizeNWSE, SizeNESW, SizeAll, UpArrow, Hand, Pin, Person) into
`HKCU\Control Panel\Cursors`.

**Important:** scheme strings store paths as `%SYSTEMROOT%\Cursors\...`. Lida expands
those environment variables before writing, because the Windows cursor loader does not
expand them itself — writing the unexpanded text is the classic reason "the registry
looks right but the pointer never changes".

After writing, Lida calls `SystemParametersInfoW(SPI_SETCURSORS, ...)` and broadcasts
`WM_SETTINGCHANGE` so running applications reload their cursors.

---

## Build

You need Visual Studio 2022 with the **Desktop development with C++** workload
(or the Build Tools).

```
build.bat
```

That compiles `src\main.cpp`, compiles the icon resource, and produces `Lida.exe`.

The icon is embedded from `icon.ico` as resource ID **101** (`resource.rc`), which is
what the app loads for the tray, taskbar and window icons. If you change the icon, keep
the resource ID at `101`.

Manual equivalent (from an x64 Native Tools prompt):

```
rc /nologo /fo resource.res resource.rc
cl /nologo /std:c++17 /EHsc /W3 /O2 /DUNICODE /D_UNICODE /utf-8 src\main.cpp resource.res ^
   user32.lib gdi32.lib shell32.lib advapi32.lib comdlg32.lib ole32.lib ^
   /link /SUBSYSTEM:WINDOWS /OUT:Lida.exe
```

---

## Troubleshooting

**Cursor does not change**

1. Confirm the scheme name is exact:
   `reg query "HKCU\Control Panel\Cursors\Schemes"`
2. Confirm the values under `HKCU\Control Panel\Cursors` are **fully expanded** paths
   (e.g. `C:\WINDOWS\Cursors\...`), not `%SYSTEMROOT%\...`.
3. Confirm the `.cur`/`.ani` files actually exist on disk.
4. Sign out and back in once — Windows caches cursor sets per session in rare cases.

**Theme is not detected**

Check that this value exists and reads `0` (dark) or `1` (light):

```
reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize" /v AppsUseLightTheme
```

**Tray icon looks like a generic Windows icon**

The icon resource is loaded by numeric ID `101`. Make sure `resource.rc` contains
`#define IDI_ICON1 101` and rebuilt with `build.bat`.

**Wallpaper does not change**

Verify the path in `config.json` exists. Use the tray menu's file picker instead of
typing paths, so they are stored in the correct form.

---

## Files

| File | Purpose |
|---|---|
| `Lida.exe` | The application |
| `config.json` | Runtime configuration (created on first run) |
| `src/main.cpp` | Entire application source |
| `resource.rc` | Icon resource definition (ID 101) |
| `icon.ico` | App / tray icon |
| `build.bat` | One-step build script |
| `README.md` | This file |

---

## Uninstall

Exit Lida from the tray menu, then delete the folder. There is no installer and no
registry footprint beyond the cursor values Lida applied (use **Restore Default
Cursors** first if you want those back to stock). To restore the original wallpaper,
pick it again in Windows Settings.
