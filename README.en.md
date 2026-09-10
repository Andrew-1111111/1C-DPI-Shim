# 1C DPI Shim

A **C++** DPI shim for the **1C:Enterprise 8.3** platform. It scales **only** the 1C UI (Designer / `1cv8.exe`). Windows scaling and every other application stay unchanged.

[Русский](README.md)

## Why this exists

On a **4K** monitor the 1C Designer (Configurator) is painfully small. The usual workaround is to raise **Windows display scaling** (for example from 150–175% to 225%). 1C becomes readable, but Firefox, Explorer, Visual Studio, and everything else become huge.

Windows cannot give **one** process a DPI higher than the system setting. Compatibility flags, `/DisableHighDpiAware`, editing the 1C manifest, and `SetProcessDpiAwarenessContext()` do not solve this: they change awareness mode, not the scale factor.

This C++ tool spoofs DPI queries **inside the 1C process**. Windows can stay at 175% while the Configurator draws at 200 / 225 / 250% or higher — including **above 255%** (up to 400%).

This is not a system DPI change, not 1C:EDT, not platform 8.2, and not a patch of the 1C install.

## Features

- Only `1cestart.exe`, `1cv8.exe`, `1cv8c.exe`, `1cv8s.exe`, `1cv8a.exe`
- Written in **C++** (single exe); no .NET / Python / PowerShell / NuGet
- MinHook is built into `source\minhook`; no external libraries
- Does not modify the 1C install
- **Win32 / x86** output (`bin\x86\`) — same bitness as 1C 8.3
- Scale **50–400%** (can exceed 255%; Windows display settings cannot do that per app)
- Scale and enable/disable via `1c-dpi.ini`
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

Tests cover percent↔DPI conversion, 1C process-name matching, launcher argument parsing, and `1c-dpi.ini` presence.

## GitHub Actions

[`.github/workflows/build.yml`](.github/workflows/build.yml) on `windows-2022`:

1. Builds **Release Win32** → `bin/x86`
2. Runs `1C_DPI_Tests.exe`
3. Packs a zip (without the test exe)
4. Uploads **Actions → Artifacts**

Runs on push to `main`/`master`, pull requests, manual **Run workflow**, and `v*` tags (e.g. `v1.0.0`). A tag publishes a **GitHub Release** with `1C-DPI-Shim-x86-Release.zip`.

## Install

Nothing is copied into `C:\Program Files (x86)\1cv8`.

1. Build Release Win32 or download the zip from Actions / Releases.
2. Copy `1C-DPI-Shim.exe` and `1c-dpi.ini` to a folder you own.
3. Optional shortcut to `1C-DPI-Shim.exe` with argument `225`.

## Run

```ini
[launcher]
exe=C:\Program Files (x86)\1cv8\common\1cestart.exe
platform_exe=C:\Program Files (x86)\1cv8\8.3.27.2342\bin\1cv8.exe
```

```bat
1C-DPI-Shim.exe 225
1C-DPI-Shim.exe 225 --designer
1C-DPI-Shim.exe 225 --exe="C:\Program Files (x86)\1cv8\common\1cestart.exe"
```

Launching stock `1cestart.exe` **without** the launcher does not load the shim.

## Configuration

See `1c-dpi.ini` next to the exe. `dpi=` is a percent of 96 DPI in the **50–400** range (values **above 255** are allowed). `dpi=225` means 216 DPI. Environment overrides: `ONEC_DPI`, `ONEC_DPI_ENABLED`, `ONEC_DPI_INI`, `ONEC_DPI_LOG`, `ONEC_DPI_LOG_ENABLED`.

## How it works

The exe starts 1C suspended, injects the embedded shim, and resumes. The shim spoofs `GetDeviceCaps` / `GetDpiForSystem` / `getContextDPI()` so 1C draws at the virtual scale. Child `1cv8*.exe` processes are injected again. Other desktop apps keep the real Windows scale. A separate `1C_DPI_Shim.dll` is not required next to the exe; the module is extracted to `%LOCALAPPDATA%\1C-DPI-Shim\` at launch.

## License

Application code: use as needed in your own environment.

MinHook (BSD): sources are part of the project (`source/minhook`); no separate library or NuGet package.
