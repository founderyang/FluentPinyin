# Developer Setup

FluentPinyin is Windows 11 x64 only.

## Required Tools

- Visual Studio 2022.
- MSVC C++ x64 toolchain.
- Windows SDK.
- CMake 3.24 or newer.

## Configure

For the normal development path, use the one-command installer. It downloads
librime and Frost if needed, builds, registers the TSF profile, and runs the
RIME smoke test:

```powershell
.\scripts\install-dev.ps1 -BuildDir .\build-rime -Config Debug
```

Manual steps are still useful when iterating on code:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

## Build

```powershell
cmake --build build --config Debug
```

## Register Development Build

```powershell
.\scripts\register-dev.ps1 -BuildDir .\build -Config Debug
```

## Unregister Development Build

```powershell
.\scripts\unregister-dev.ps1 -BuildDir .\build -Config Debug
```

## Logs

Logs are written under:

```text
%LOCALAPPDATA%\FluentPinyin\Logs\
```

## RIME Dictionary Import

Only RIME `.dict.yaml` files are supported by `fp-lexicon-import.exe`:

```powershell
.\build-rime\bin\Debug\fp-lexicon-import.exe .\my_words.dict.yaml
.\build-rime\bin\Debug\fp-rime-smoke.exe nihao
```
