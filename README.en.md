# 1C DPI Shim

A **C++** DPI shim for the **1C:Enterprise 8.3** platform. It scales **only** the 1C UI (Designer / `1cv8.exe`). Windows scaling and every other application stay unchanged.

[Русский](README.md)

## Why this exists

On a **4K** monitor the 1C Designer (Configurator) is painfully small. The usual workaround is to raise **Windows display scaling** (for example from 150–175% to 200%). 1C becomes readable, but Firefox, Explorer, Visual Studio, and everything else become huge.

Windows cannot give **one** process a DPI higher than the system setting. Compatibility flags, `/DisableHighDpiAware`, editing the 1C manifest, and `SetProcessDpiAwarenessContext()` do not solve this: they change awareness mode, not the scale factor.

This C++ tool spoofs DPI queries **inside the 1C process**. Windows can stay at 175% while the Configurator draws at 200 / 225 / 250% or higher — up to **500%**.

This is not a system DPI change, not 1C:EDT, not platform 8.2, and not a patch of the 1C install.

## Features

- Only `1cestart.exe`, `1cv8.exe`, `1cv8c.exe`, `1cv8s.exe`, `1cv8a.exe`
- Written in **C++**; no .NET / Python / PowerShell / NuGet
- Ship `1C-DPI-Shim.exe` and `1c-dpi.ini`. At launch `1C_DPI_Shim.dll` appears next to the exe (Windows can load a DLL only from disk). Nothing is written under `%LOCALAPPDATA%`
- MinHook is built into `source\minhook`; no external libraries
- Does not modify the 1C install
- **Win32 / x86** output (`bin\x86\`) — same bitness as 1C 8.3
- Scale only **100 / 125 / 150 / 175 / 200 / 225 / 250 / 300 / 400 / 500%** (stock Windows steps)
- Scale and 1C paths via `1c-dpi.ini` or the command line
- Run from a console, a shortcut with arguments, or double-click (defaults from the ini)
- Diagnostic log next to the exe

x86 only: they must match `1cv8.exe`, and 1C:Enterprise 8.3 is 32-bit (`C:\Program Files (x86)\1cv8\...`).

## Requirements

- Windows 10/11
- Visual Studio 2022 (v143) or later (v145)
- **1C:Enterprise 8.3 x86** platform (tested with 8.3.27.2342)

## Build

Open [`1C-DPI-Shim.sln`](1C-DPI-Shim.sln), platform **Win32**.

Startup project: `1C_DPI_Shim` (builds `1C-DPI-Shim.exe` with the shim embedded). Build: Ctrl+Shift+B → `bin\x86\`.

```bat
msbuild 1C-DPI-Shim.sln /p:Configuration=Release /p:Platform=Win32
```

Output:

```text
bin\x86\1C-DPI-Shim.exe
bin\x86\1c-dpi.ini
bin\x86\1C_DPI_Tests.exe
```

```bat
bin\x86\1C_DPI_Tests.exe
```

Tests cover percent↔DPI conversion, 1C process-name matching, command-line parsing, 1C paths from the ini and command line, and `1c-dpi.ini` presence.

## GitHub Actions

[`.github/workflows/build.yml`](.github/workflows/build.yml) on `windows-2022`:

1. Builds **Release Win32** → `bin/x86`
2. Runs `1C_DPI_Tests.exe`
3. Packs a zip (without the test exe)
4. Uploads **Actions → Artifacts**

Runs on push to `main`/`master`, pull requests, manual **Run workflow**, and `v*` tags (e.g. `v1.0.0`). A tag publishes a **GitHub Release** with `1C-DPI-Shim-x86-Release.zip`.

## Install

Nothing is copied into `C:\Program Files (x86)\1cv8`.

1. Build Release Win32 or download the zip from [Releases](https://github.com/Andrew-1111111/1C-DPI-Shim/releases) / Actions.
2. Copy `1C-DPI-Shim.exe` and `1c-dpi.ini` to a folder you own.

On the first launch `1C_DPI_Shim.dll` appears in the same folder. If that folder is not writable, the DLL is written to `%TEMP%`.

Then use any of three ways: console, a shortcut with arguments, or double-click with ini defaults.

## How to run

Command-line arguments override `1c-dpi.ini` (`--exe` for the 1C path, a percent / `--dpi` for scale, `--ini` for the settings file). Launching stock `1cestart.exe` **without** `1C-DPI-Shim.exe` does not load the shim.

### Console

In `cmd` or PowerShell, `cd` to the folder with the exe and ini:

```bat
cd /d C:\Tools\1C-DPI-Shim
1C-DPI-Shim.exe --help
1C-DPI-Shim.exe 200
1C-DPI-Shim.exe 200 --designer
1C-DPI-Shim.exe 200 --exe="C:\Program Files (x86)\1cv8\common\1cestart.exe"
1C-DPI-Shim.exe 250 --designer
1C-DPI-Shim.exe --console
```

`--console` keeps a console window with launch status. Otherwise a console appears only on errors or `--help`.

### Shortcut with settings

1. Right-click `1C-DPI-Shim.exe` → **Create shortcut**.
2. Shortcut **Properties** → **Target**, for example:

```text
"C:\Tools\1C-DPI-Shim\1C-DPI-Shim.exe" 200 --designer
```

3. **Start in** — the folder that contains the exe and `1c-dpi.ini`.

You can keep several shortcuts (Designer at 200%, Enterprise at 150%, and so on) and pin them to the taskbar or desktop.

### Defaults from the ini

Double-click `1C-DPI-Shim.exe` **with no arguments**. Scale, the 1C path, and the other options come from `1c-dpi.ini` next to the exe.

Edit and save the ini; the next launch picks up the values. A shortcut with arguments is optional.

```ini
[shim]
dpi=200
enabled=1

[launcher]
exe=C:\Program Files (x86)\1cv8\common\1cestart.exe
platform_exe=C:\Program Files (x86)\1cv8\8.3.27.2342\bin\1cv8.exe
```

- `dpi=` — 1C scale when no percent is given on the command line. **Only** the values in the table below (default 200).
- `exe=` / `start_exe=` — `1cestart.exe` for a normal start (`--start`).
- `platform_exe=` — `1cv8.exe` when you use `--designer`.
- `--exe=` overrides `exe` / `start_exe` / `platform_exe`.

If the ini paths are empty, the program looks for 1C under the default `Program Files (x86)\1cv8` folders.

## Configuration

See `1c-dpi.ini` next to the exe. `dpi=` accepts **only** these stock Windows scale steps:

| Percent | DPI |
| --- | --- |
| 100 | 96 |
| 125 | 120 |
| 150 | 144 |
| 175 | 168 |
| 200 | 192 |
| 225 | 216 |
| 250 | 240 |
| 300 | 288 |
| 400 | 384 |
| 500 | 480 |

Arbitrary percents (for example 230) are rejected. Environment variables (override the ini; the program also sets them for 1C): `ONEC_DPI`, `ONEC_DPI_ENABLED`, `ONEC_DPI_EXE`, `ONEC_DPI_INI`, `ONEC_DPI_LOG`, `ONEC_DPI_LOG_ENABLED`.

## Log

Default path: `<program folder>\1c-dpi-shim.log`.

```ini
log=0
```

or `1C-DPI-Shim.exe 200 --no-log`.

Successful launch with Windows at 175% and the shim at 200%:

```text
system_dpi=168 (175%) virtual_dpi=192 (200%)
spoof_active=yes
GetDeviceCaps(LOGPIXELSX) original=168 spoofed=192 spoof=yes
```

## Uninstall

Close 1C, do not start the program, and delete the folder with `1C-DPI-Shim.exe`, `1c-dpi.ini`, the log, and `1C_DPI_Shim.dll`. If the DLL was written to `%TEMP%`, delete it there too. Stock 1C shortcuts were not changed. Quick disable: `enabled=0`.

## How it works

The exe starts 1C suspended, injects the embedded shim, and resumes. The shim spoofs `GetDeviceCaps` / `GetDpiForSystem` / `getContextDPI()` so 1C draws at the virtual scale. Child `1cv8*.exe` processes are injected again. Other desktop apps keep the real Windows scale. The shim module is embedded in the exe and extracted **next to it** at launch (`1C_DPI_Shim.dll`) because Windows can inject a DLL only from disk.

## Limitations

- 1C must be started through `1C-DPI-Shim.exe`
- Changing `dpi` requires restarting 1C
- Some UI (WebKit, external components, system dialogs) may stay at the system scale
- Policies / antivirus may block `CreateRemoteThread`

## License

Application code: use as needed in your own environment.

MinHook (BSD): sources are part of the project (`source/minhook`); no separate library or NuGet package.
