# FluentPinyin

FluentPinyin is a Windows 11 x64 Chinese input method. The public repository
contains source code, installer definitions, and local build scripts only.
Downloaded dependencies, bundled input data, fonts, and release artifacts are
kept out of Git and prepared locally before building.

## Build

Requirements:

- Windows 11 x64
- Visual Studio 2022 Build Tools with MSVC x64
- CMake 3.24 or newer
- Python 3 with Pillow for icon generation
- WiX Toolset 7
- GitHub CLI for publishing releases

If WiX asks for an EULA acknowledgement on first use, run:

```powershell
wix eula accept wix7
```

Prepare local dependencies:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\prepare-packages.ps1
python .\scripts\generate-icons.py
```

Build locally:

```powershell
cmake -S . -B .\build-release -G "Visual Studio 17 2022" -A x64
cmake --build .\build-release --config Release --parallel
```

Create release package:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-release.ps1
```

Generated files are written to `dist\release`:

- `FluentPinyin.msi`

The installer uses `C:\Program Files\FluentPinyin`.

## Update

The About page update button downloads the latest GitHub Release asset named
`FluentPinyin.msi`.

## Third-Party Components

See `third_party/README.md` for bundled component, font, icon, and source
information.
