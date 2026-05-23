param(
    [string]$BuildDir = ".\build-input-hardening",
    [string]$Config = "Release",
    [string]$OutputDir = ".\dist\FluentPinyin"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $resolvedBuildDir = (Resolve-Path -Path $BuildDir -ErrorAction Stop).ProviderPath
    $binDir = Join-Path $resolvedBuildDir "bin\$Config"
    if (-not (Test-Path -LiteralPath $binDir)) {
        throw "Cannot find build output: $binDir"
    }

    $resolvedOutput = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
    if (Test-Path -LiteralPath $resolvedOutput) {
        Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $resolvedOutput | Out-Null

    $files = @(
        "fp-tsf.dll",
        "fp-core.dll",
        "fp-ui.exe",
        "fp-config.exe",
        "fp-updater.exe",
        "fp-rime-smoke.exe",
        "fp-lexicon-import.exe",
        "rime.dll"
    )

    foreach ($file in $files) {
        $source = Join-Path $binDir $file
        if (-not (Test-Path -LiteralPath $source)) {
            throw "Missing build artifact: $source"
        }
        Copy-Item -LiteralPath $source -Destination (Join-Path $resolvedOutput $file) -Force
    }

    $sourceRimeData = Join-Path $binDir "rime-data"
    if (-not (Test-Path -LiteralPath $sourceRimeData)) {
        throw "Missing RIME data directory: $sourceRimeData"
    }
    Copy-Item -LiteralPath $sourceRimeData -Destination (Join-Path $resolvedOutput "rime-data") -Recurse -Force

    $scriptsDir = Join-Path $resolvedOutput "scripts"
    New-Item -ItemType Directory -Force -Path $scriptsDir | Out-Null
    Copy-Item -LiteralPath ".\scripts\install-user.ps1" -Destination (Join-Path $scriptsDir "install-user.ps1") -Force
    Copy-Item -LiteralPath ".\scripts\uninstall-user.ps1" -Destination (Join-Path $scriptsDir "uninstall-user.ps1") -Force

    @(
        "@echo off",
        "setlocal",
        'cd /d "%~dp0"',
        'powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install-user.ps1" -PackageDir "%~dp0"',
        "pause"
    ) | Set-Content -LiteralPath (Join-Path $resolvedOutput "install.bat") -Encoding ascii

    @(
        "@echo off",
        "setlocal",
        'cd /d "%~dp0"',
        'powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\uninstall-user.ps1" -KeepUserData',
        "pause"
    ) | Set-Content -LiteralPath (Join-Path $resolvedOutput "uninstall.bat") -Encoding ascii

    $readmeBase64 = "5rWB55WF5ou86Z+zDQoNCuWPjOWHuyBpbnN0YWxsLmJhdCDlronoo4XjgIINCuWPjOWHuyB1bmluc3RhbGwuYmF0IOWNuOi9ve+8jOm7mOiupOS/neeVmeeUqOaIt+ivjeW6k+WSjOmFjee9ruOAgg0KDQrovpPlhaXlhoXlrrnnlLHmnKzlnLAgUklNRSDlvJXmk47lpITnkIbjgILmo4Dmn6Uv5LiL6L295pu05paw5Y+q5Lya5Zyo55So5oi35Li75Yqo54K55Ye75pe26IGU572R44CCDQo="
    [IO.File]::WriteAllBytes((Join-Path $resolvedOutput "README.txt"), [Convert]::FromBase64String($readmeBase64))

    $archive = Join-Path $root "dist\FluentPinyin.zip"
    Compress-Archive -Path (Join-Path $resolvedOutput "*") -DestinationPath $archive -Force
    Write-Host "Packaged FluentPinyin to $resolvedOutput"
    Write-Host "Archive: $archive"
} finally {
    Pop-Location
}
