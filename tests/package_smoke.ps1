param(
  [Parameter(Mandatory = $true)]
  [string]$PayloadDir,

  [Parameter(Mandatory = $true)]
  [string]$ReleaseDir,

  [double]$MaxPayloadMb = 0,

  [switch]$RequireMsi
)

$ErrorActionPreference = "Stop"

function Assert-PathExists {
  param([string]$Path)
  if (-not (Test-Path -LiteralPath $Path)) {
    throw "Missing expected path: $Path"
  }
}

function Assert-True {
  param(
    [bool]$Condition,
    [string]$Message
  )
  if (-not $Condition) {
    throw $Message
  }
}

foreach ($file in @(
  "fluent-pinyin-core.dll",
  "fluent-pinyin-tsf.dll",
  "fluent-pinyin-devtools.exe",
  "fluent-pinyin-corehost.exe",
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

$rimeDataDir = Join-Path $PayloadDir "rime-data"
foreach ($file in @(
  "default.yaml",
  "wanxiang.schema.yaml",
  "wanxiang.dict.yaml",
  "wanxiang-lts-zh-hans.gram"
)) {
  Assert-PathExists (Join-Path $rimeDataDir $file)
}

$grammar = Get-Item -LiteralPath (Join-Path $rimeDataDir "wanxiang-lts-zh-hans.gram")
if ($grammar.Length -lt 200MB) {
  throw "Wanxiang grammar model looks missing or truncated: $($grammar.Length) bytes"
}

$yamlFiles = Get-ChildItem -LiteralPath $rimeDataDir -Recurse -File -Include "*.yaml"
foreach ($yaml in $yamlFiles) {
  $yamlText = Get-Content -LiteralPath $yaml.FullName -Raw -Encoding UTF8
  if ($yamlText -match "FluentPinyin managed") {
    throw "Packaged upstream Rime data must not contain runtime managed patch text: $($yaml.FullName)"
  }
}

$fontFiles = Get-ChildItem -LiteralPath (Join-Path $PayloadDir "fonts") -File
if ($fontFiles.Count -eq 0) {
  throw "Payload fonts directory is empty"
}
foreach ($fontPattern in @("MiSans*.ttf", "SourceHanSans*.otf", "Plangothic*.ttf")) {
  $matches = @($fontFiles | Where-Object { $_.Name -like $fontPattern })
  if ($matches.Count -eq 0) {
    throw "Payload fonts directory is missing required private font pattern: $fontPattern"
  }
}
foreach ($fontName in @("MiSans-Regular.ttf", "MiSans-Medium.ttf", "MiSans-Semibold.ttf")) {
  $matches = @($fontFiles | Where-Object { $_.Name -eq $fontName })
  if ($matches.Count -eq 0) {
    throw "Payload fonts directory is missing settings UI MiSans font: $fontName"
  }
}

$report = Join-Path $ReleaseDir "payload-size-report.txt"
Assert-PathExists $report
$reportText = Get-Content -LiteralPath $report -Raw
foreach ($required in @("Payload size report:", "rime-data:", "fonts:", "Largest payload files:")) {
  if ($reportText -notmatch [regex]::Escape($required)) {
    throw "Payload report missing '$required'"
  }
}
if ($MaxPayloadMb -gt 0) {
  $totalSize = (Get-ChildItem -LiteralPath $PayloadDir -Recurse -File |
      Measure-Object -Property Length -Sum).Sum
  $totalMb = $totalSize / 1024 / 1024
  if ($totalMb -gt $MaxPayloadMb) {
    throw ("Payload size {0:N1} MB exceeds limit {1:N1} MB" -f $totalMb, $MaxPayloadMb)
  }
}

$resourceManifest = Join-Path $ReleaseDir "payload-resource-manifest.json"
Assert-PathExists $resourceManifest
$manifest = Get-Content -LiteralPath $resourceManifest -Raw -Encoding UTF8 | ConvertFrom-Json
Assert-True ([int64]$manifest.total_size -gt 0) "Resource manifest total size is empty"
$manifestPaths = @($manifest.files | ForEach-Object { $_.path })
foreach ($requiredPath in @(
  "fluent-pinyin-corehost.exe",
  "rime-data/wanxiang-lts-zh-hans.gram"
)) {
  if ($manifestPaths -notcontains $requiredPath) {
    throw "Resource manifest missing required file: $requiredPath"
  }
}
foreach ($category in @("rime-data", "fonts", "binary")) {
  $matches = @($manifest.files | Where-Object { $_.category -eq $category })
  if ($matches.Count -eq 0) {
    throw "Resource manifest missing category: $category"
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
  $installerWxs = Join-Path (Split-Path -Parent $PSScriptRoot) "installer\fluent-pinyin.wxs"
  Assert-PathExists $installerWxs
  $installerWxsText = Get-Content -LiteralPath $installerWxs -Raw
  foreach ($required in @(
    '<Property Id="REBOOT" Value="ReallySuppress" />',
    '<Property Id="MSIRESTARTMANAGERCONTROL" Value="DisableShutdown" />'
  )) {
    if ($installerWxsText -notmatch [regex]::Escape($required)) {
      throw "Installer WiX manifest missing restart prompt suppression: $required"
    }
  }
  Assert-PathExists (Join-Path $ReleaseDir "FluentPinyin.msi")
}

Write-Host "Package smoke test passed"
