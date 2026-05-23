param(
    [string]$InstallDir = "$env:LOCALAPPDATA\Programs\FluentPinyin",
    [switch]$KeepUserData
)

$ErrorActionPreference = "Stop"

$dll = Join-Path $InstallDir "fp-tsf.dll"
if (Test-Path -LiteralPath $dll) {
    $regsvr32 = Join-Path $env:SystemRoot "System32\regsvr32.exe"
    $process = Start-Process -FilePath $regsvr32 -ArgumentList @("/u", "/s", $dll) -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        throw "regsvr32 /u failed with exit code $($process.ExitCode)"
    }
}

if (Test-Path -LiteralPath $InstallDir) {
    Remove-Item -LiteralPath $InstallDir -Recurse -Force
}

if (-not $KeepUserData) {
    $roaming = Join-Path $env:APPDATA "FluentPinyin"
    $local = Join-Path $env:LOCALAPPDATA "FluentPinyin"
    foreach ($path in @($roaming, $local)) {
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }
}

Write-Host "FluentPinyin uninstalled."
