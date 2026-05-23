param(
    [string]$BuildDir = ".\build-rime",
    [string]$Config = "Release",
    [string]$PackageDir = "",
    [string]$InstallDir = "$env:LOCALAPPDATA\Programs\FluentPinyin",
    [switch]$SkipRegister
)

$ErrorActionPreference = "Stop"

if (-not [Environment]::Is64BitOperatingSystem) {
    throw "FluentPinyin supports Windows 11 x64 only."
}

$root = Split-Path -Parent $PSScriptRoot
$initialLocation = (Get-Location).ProviderPath
Push-Location $root
try {
    if ([string]::IsNullOrWhiteSpace($PackageDir)) {
        $resolvedBuildDir = (Resolve-Path -Path $BuildDir -ErrorAction Stop).ProviderPath
        $sourceDir = Join-Path $resolvedBuildDir "bin\$Config"
        if (-not (Test-Path -LiteralPath $sourceDir)) {
            throw "Cannot find build output: $sourceDir"
        }
    } else {
        if ([IO.Path]::IsPathRooted($PackageDir)) {
            $packagePath = $PackageDir
        } else {
            $packagePath = Join-Path $initialLocation $PackageDir
        }
        $sourceDir = (Resolve-Path -Path $packagePath -ErrorAction Stop).ProviderPath
        if (-not (Test-Path -LiteralPath $sourceDir)) {
            throw "Cannot find package directory: $sourceDir"
        }
    }

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

    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    foreach ($file in $files) {
        $source = Join-Path $sourceDir $file
        if (-not (Test-Path -LiteralPath $source)) {
            throw "Missing install artifact: $source"
        }
        Copy-Item -LiteralPath $source -Destination (Join-Path $InstallDir $file) -Force
    }

    $sourceRimeData = Join-Path $sourceDir "rime-data"
    $targetRimeData = Join-Path $InstallDir "rime-data"
    if (-not (Test-Path -LiteralPath $sourceRimeData)) {
        throw "Missing RIME data directory: $sourceRimeData"
    }
    if (Test-Path -LiteralPath $targetRimeData) {
        Remove-Item -LiteralPath $targetRimeData -Recurse -Force
    }
    Copy-Item -LiteralPath $sourceRimeData -Destination $targetRimeData -Recurse -Force

    if (-not $SkipRegister) {
        $regsvr32 = Join-Path $env:SystemRoot "System32\regsvr32.exe"
        $dll = Join-Path $InstallDir "fp-tsf.dll"
        $process = Start-Process -FilePath $regsvr32 -ArgumentList @("/s", $dll) -Wait -PassThru
        if ($process.ExitCode -ne 0) {
            throw "regsvr32 failed with exit code $($process.ExitCode)"
        }
    }

    $config = Join-Path $InstallDir "fp-config.exe"
    Write-Host "FluentPinyin installed to $InstallDir"
    Write-Host "Settings: $config"
} finally {
    Pop-Location
}
