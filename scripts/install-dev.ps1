param(
    [string]$BuildDir = ".\build-dev",
    [string]$Config = "Debug",
    [switch]$SkipPackagePrepare,
    [switch]$SkipRegister
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    if (-not $SkipPackagePrepare) {
        & .\scripts\prepare-packages.ps1
    }

    $env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
        [Environment]::GetEnvironmentVariable("Path", "User")

    cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64
    cmake --build $BuildDir --config $Config

    if (-not $SkipRegister) {
        & .\scripts\register-dev.ps1 -BuildDir $BuildDir -Config $Config
    }

    $smoke = Join-Path $BuildDir "bin\$Config\fp-rime-smoke.exe"
    if (Test-Path -LiteralPath $smoke) {
        & $smoke nihao
        if ($LASTEXITCODE -ne 0) {
            throw "fp-rime-smoke failed with exit code $LASTEXITCODE"
        }
    }

    Write-Host "FluentPinyin development install completed."
} finally {
    Pop-Location
}
