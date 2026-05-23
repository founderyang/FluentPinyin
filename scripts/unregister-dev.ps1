param(
    [string]$BuildDir = ".\build",
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$resolvedBuildDir = Resolve-Path -Path $BuildDir -ErrorAction Stop
$candidateDlls = @(
    (Join-Path $resolvedBuildDir "bin\$Config\fp-tsf.dll"),
    (Join-Path $resolvedBuildDir "bin\fp-tsf.dll")
)
$dll = $candidateDlls | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

if (-not $dll) {
    throw "Cannot find TSF DLL under $resolvedBuildDir."
}

$regsvr32 = Join-Path $env:SystemRoot "System32\regsvr32.exe"
Write-Host "Unregistering $dll"
$process = Start-Process -FilePath $regsvr32 -ArgumentList @("/u", "/s", $dll) -Wait -PassThru

if ($process.ExitCode -ne 0) {
    throw "regsvr32 /u failed with exit code $($process.ExitCode)"
}

Write-Host "FluentPinyin development TSF DLL unregistered."
