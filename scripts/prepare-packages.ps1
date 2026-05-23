param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$downloadsDir = Join-Path $root "packages\downloads"
$librimeDir = Join-Path $root "third_party\librime"
$frostDir = Join-Path $root "schemas\frost\current"
$assetsPath = Join-Path $downloadsDir "m2-assets.json"

New-Item -ItemType Directory -Force -Path $downloadsDir, $librimeDir | Out-Null

if (-not (Test-Path -LiteralPath $assetsPath)) {
    @{
        librime = "rime-de4700e-Windows-msvc-x64.7z"
        librimeUrl = "https://github.com/rime/librime/releases/download/1.16.1/rime-de4700e-Windows-msvc-x64.7z"
        deps = "rime-deps-de4700e-Windows-msvc-x64.7z"
        depsUrl = "https://github.com/rime/librime/releases/download/1.16.1/rime-deps-de4700e-Windows-msvc-x64.7z"
        frost = "rime-frost-schemas.zip"
        frostUrl = "https://github.com/gaboolic/rime-frost/releases/download/1.0.4/rime-frost-schemas.zip"
    } | ConvertTo-Json | Set-Content -Path $assetsPath -Encoding UTF8
}

$assets = Get-Content -Path $assetsPath | ConvertFrom-Json
$downloads = @(
    @{ Name = $assets.librime; Url = $assets.librimeUrl; Dest = Join-Path $librimeDir "rime" },
    @{ Name = $assets.deps; Url = $assets.depsUrl; Dest = Join-Path $librimeDir "deps" },
    @{ Name = $assets.frost; Url = $assets.frostUrl; Dest = $frostDir }
)

foreach ($item in $downloads) {
    $archive = Join-Path $downloadsDir $item.Name
    if ($Force -or -not (Test-Path -LiteralPath $archive)) {
        Write-Host "Downloading $($item.Url)"
        Invoke-WebRequest -Uri $item.Url -OutFile $archive
    } else {
        Write-Host "Using cached $archive"
    }

    if ($Force -or -not (Test-Path -LiteralPath $item.Dest)) {
        if (Test-Path -LiteralPath $item.Dest) {
            Remove-Item -LiteralPath $item.Dest -Recurse -Force
        }
        New-Item -ItemType Directory -Force -Path $item.Dest | Out-Null
        Write-Host "Extracting $archive -> $($item.Dest)"
        tar -xf $archive -C $item.Dest
    } else {
        Write-Host "Using extracted $($item.Dest)"
    }
}

Write-Host "Packages are ready."
