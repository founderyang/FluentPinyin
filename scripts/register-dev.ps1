param(
    [string]$BuildDir = ".\build",
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

if (-not [Environment]::Is64BitOperatingSystem) {
    throw "FluentPinyin supports Windows 11 x64 only."
}

$resolvedBuildDir = Resolve-Path -Path $BuildDir -ErrorAction Stop
$candidateDlls = @(
    (Join-Path $resolvedBuildDir "bin\$Config\fp-tsf.dll"),
    (Join-Path $resolvedBuildDir "bin\fp-tsf.dll")
)
$dll = $candidateDlls | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

if (-not $dll) {
    throw "Cannot find TSF DLL under $resolvedBuildDir. Build the project first."
}

$regsvr32 = Join-Path $env:SystemRoot "System32\regsvr32.exe"
Write-Host "Registering $dll"
$process = Start-Process -FilePath $regsvr32 -ArgumentList @("/s", $dll) -Wait -PassThru

if ($process.ExitCode -ne 0) {
    throw "regsvr32 failed with exit code $($process.ExitCode)"
}

Write-Host "FluentPinyin development TSF DLL registered."
