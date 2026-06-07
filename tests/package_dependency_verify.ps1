param(
  [string]$ManifestPath = (Join-Path $PSScriptRoot "..\scripts\package_dependencies.json"),
  [string]$DownloadsDir = (Join-Path $PSScriptRoot "..\packages\downloads")
)

$ErrorActionPreference = "Stop"

function Get-Sha256Hex {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path
  )

  $stream = [System.IO.File]::OpenRead((Resolve-Path -LiteralPath $Path).Path)
  try {
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
      $bytes = $sha256.ComputeHash($stream)
      return ([System.BitConverter]::ToString($bytes) -replace "-", "").ToLowerInvariant()
    } finally {
      $sha256.Dispose()
    }
  } finally {
    $stream.Dispose()
  }
}

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

  $hash = Get-Sha256Hex -Path $path
  if ($hash -ne [string]$dependency.sha256) {
    throw "Dependency SHA256 mismatch for $($dependency.id): expected $($dependency.sha256), got $hash"
  }

  Write-Host "Verified $($dependency.id): $($dependency.name)"
}

Write-Host "Package dependency cache verified."
