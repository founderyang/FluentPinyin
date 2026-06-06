param(
  [Parameter(Mandatory = $true)]
  [string]$PayloadDir,

  [Parameter(Mandatory = $true)]
  [string]$ReleaseDir,

  [switch]$RequireMsi
)

$ErrorActionPreference = "Stop"

function Assert-PathExists {
  param([string]$Path)
  if (-not (Test-Path -LiteralPath $Path)) {
    throw "Missing expected path: $Path"
  }
}

foreach ($file in @(
  "fluent-pinyin-core.dll",
  "fluent-pinyin-tsf.dll",
  "fluent-pinyin-devtools.exe",
  "fluent-pinyin-updater.exe",
  "rime.dll",
  "README.txt",
  "LICENSE.txt"
)) {
  Assert-PathExists (Join-Path $PayloadDir $file)
}

foreach ($dir in @(
  "rime-data",
  "fonts",
  "THIRD_PARTY_LICENSES"
)) {
  Assert-PathExists (Join-Path $PayloadDir $dir)
}

$report = Join-Path $ReleaseDir "payload-size-report.txt"
Assert-PathExists $report
$reportText = Get-Content -LiteralPath $report -Raw
foreach ($required in @("Payload size report:", "rime-data:", "fonts:", "Largest payload files:")) {
  if ($reportText -notmatch [regex]::Escape($required)) {
    throw "Payload report missing '$required'"
  }
}

if ($RequireMsi) {
  Assert-PathExists (Join-Path $ReleaseDir "FluentPinyin.msi")
}

Write-Host "Package smoke test passed"
