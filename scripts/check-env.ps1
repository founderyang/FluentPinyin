$ErrorActionPreference = "Continue"

$env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
    [Environment]::GetEnvironmentVariable("Path", "User")

Write-Host "FluentPinyin environment check"
Write-Host "OS: $([Environment]::OSVersion.VersionString)"
Write-Host "64-bit OS: $([Environment]::Is64BitOperatingSystem)"
Write-Host "64-bit process: $([Environment]::Is64BitProcess)"

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) {
    Write-Host "cmake: $($cmake.Source)"
    cmake --version
} else {
    Write-Warning "cmake was not found in PATH."
}

$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if ($cl) {
    Write-Host "cl.exe: $($cl.Source)"
} else {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) {
            Write-Host "Visual Studio Build Tools: $vsPath"
            Write-Warning "cl.exe is available through the Visual Studio generator or Developer PowerShell, but not directly in this shell PATH."
        } else {
            Write-Warning "MSVC x64 tools were not found by vswhere."
        }
    } else {
        Write-Warning "cl.exe was not found in PATH. Open a Visual Studio Developer PowerShell or install MSVC tools."
    }
}
