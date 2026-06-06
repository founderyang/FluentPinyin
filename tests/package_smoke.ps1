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
  "fluent-pinyin-settings.exe",
  "windowsappruntimeinstall-x64.exe",
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

$fontFiles = Get-ChildItem -LiteralPath (Join-Path $PayloadDir "fonts") -File
if ($fontFiles.Count -eq 0) {
  throw "Payload fonts directory is empty"
}

$report = Join-Path $ReleaseDir "payload-size-report.txt"
Assert-PathExists $report
$reportText = Get-Content -LiteralPath $report -Raw
foreach ($required in @("Payload size report:", "rime-data:", "fonts:", "Largest payload files:")) {
  if ($reportText -notmatch [regex]::Escape($required)) {
    throw "Payload report missing '$required'"
  }
}

$payloadWxs = Join-Path $ReleaseDir "payload.wxs"
Assert-PathExists $payloadWxs
$payloadWxsText = Get-Content -LiteralPath $payloadWxs -Raw
foreach ($forbidden in @(
  "<Font",
  "FontResource",
  "CurrentVersion\Fonts",
  "Microsoft\Windows\Fonts"
)) {
  if ($payloadWxsText -match [regex]::Escape($forbidden)) {
    throw "Payload WiX manifest must not install fonts through system font registration: $forbidden"
  }
}
foreach ($font in $fontFiles) {
  $relativeFontPath = "fonts\" + $font.Name
  if ($payloadWxsText -notmatch [regex]::Escape($relativeFontPath)) {
    throw "Payload WiX manifest missing private font file path: $relativeFontPath"
  }
}

if ($payloadWxsText -notmatch [regex]::Escape("windowsappruntimeinstall-x64.exe")) {
  throw "Payload WiX manifest missing Windows App Runtime installer"
}

if ($RequireMsi) {
  Assert-PathExists (Join-Path $ReleaseDir "FluentPinyin.msi")
}

Write-Host "Package smoke test passed"
