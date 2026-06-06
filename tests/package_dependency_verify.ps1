param(
  [string]$ManifestPath = (Join-Path $PSScriptRoot "..\scripts\package_dependencies.json"),
  [string]$DownloadsDir = (Join-Path $PSScriptRoot "..\packages\downloads")
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
  throw "Missing dependency manifest: $ManifestPath"
}

$dependencies = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($dependency in $dependencies) {
  $path = Join-Path $DownloadsDir $dependency.name
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
    throw "Missing dependency $($dependency.id): $path"
  }

  $file = Get-Item -LiteralPath $path
  if ($file.Length -ne [int64]$dependency.size) {
    throw "Dependency size mismatch for $($dependency.id): expected $($dependency.size), got $($file.Length)"
  }

  $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
  if ($hash -ne [string]$dependency.sha256) {
    throw "Dependency SHA256 mismatch for $($dependency.id): expected $($dependency.sha256), got $hash"
  }

  Write-Host "Verified $($dependency.id): $($dependency.name)"
}

Write-Host "Package dependency cache verified."
