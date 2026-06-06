param(
  [Parameter(Mandatory = $true)]
  [string]$BinDir,

  [Parameter(Mandatory = $true)]
  [string]$GeneratedDir,

  [Parameter(Mandatory = $true)]
  [string]$ExpectedVersion
)

$ErrorActionPreference = "Stop"

function Assert-FileExists {
  param([string]$Path)
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    throw "Missing expected file: $Path"
  }
}

function Assert-ProcessExitCode {
  param(
    [string]$FilePath,
    [string[]]$ArgumentList,
    [int]$Expected
  )
  if ($ArgumentList -and $ArgumentList.Count -gt 0) {
    $process = Start-Process -FilePath $FilePath -ArgumentList $ArgumentList -Wait -PassThru -WindowStyle Hidden
  } else {
    $process = Start-Process -FilePath $FilePath -Wait -PassThru -WindowStyle Hidden
  }
  if ($process.ExitCode -ne $Expected) {
    throw "Expected exit code $Expected from $FilePath, got $($process.ExitCode)"
  }
}

$requiredFiles = @(
  "fluent-pinyin-core.dll",
  "fluent-pinyin-tsf.dll",
  "fluent-pinyin-ui.exe",
  "fluent-pinyin-settings.exe",
  "fluent-pinyin-updater.exe",
  "fluent-pinyin-devtools.exe",
  "rime.dll"
)

foreach ($file in $requiredFiles) {
  Assert-FileExists (Join-Path $BinDir $file)
}

$constants = Join-Path $GeneratedDir "common\constants.h"
Assert-FileExists $constants
$constantsText = Get-Content -LiteralPath $constants -Raw
if ($constantsText -notmatch [regex]::Escape("kProductVersion = L`"$ExpectedVersion`"")) {
  throw "Generated constants.h does not contain expected version $ExpectedVersion"
}

$updater = Join-Path $BinDir "fluent-pinyin-updater.exe"
$devtools = Join-Path $BinDir "fluent-pinyin-devtools.exe"
Assert-ProcessExitCode -FilePath $updater -Expected 0
Assert-ProcessExitCode -FilePath $devtools -Expected 1

$releaseJson = Join-Path $env:TEMP "fluent-pinyin-release-smoke.json"
@'
{
  "assets": [
    {
      "name": "FluentPinyin-symbols.zip",
      "browser_download_url": "https://example.invalid/wrong.msi",
      "metadata": { "name": "FluentPinyin.msi" }
    },
    {
      "browser_download_url": "https://example.invalid/downloads/FluentPinyin.msi?label=FluentPinyin%20MSI",
      "name": "FluentPinyin.msi"
    }
  ],
  "tag_name": "v00.00.04",
  "nested": { "tag_name": "v99.99.99" },
  "escaped": "quote: \" slash: \\ unicode: \u6d41\u7545"
}
'@ | Set-Content -LiteralPath $releaseJson -Encoding UTF8

$parseOutputFile = Join-Path $env:TEMP "fluent-pinyin-release-smoke.out"
$parseErrorFile = Join-Path $env:TEMP "fluent-pinyin-release-smoke.err"
$parseProcess = Start-Process -FilePath $updater `
  -ArgumentList @("parse-release-json", $releaseJson, "FluentPinyin.msi") `
  -Wait `
  -PassThru `
  -WindowStyle Hidden `
  -RedirectStandardOutput $parseOutputFile `
  -RedirectStandardError $parseErrorFile
if ($parseProcess.ExitCode -ne 0) {
  $parseError = ""
  if (Test-Path -LiteralPath $parseErrorFile) {
    $parseError = Get-Content -LiteralPath $parseErrorFile -Raw
  }
  throw "Release JSON parser smoke failed with exit code $($parseProcess.ExitCode): $parseError"
}
$parseOutput = ""
if (Test-Path -LiteralPath $parseOutputFile) {
  $parseOutput = Get-Content -LiteralPath $parseOutputFile -Raw
}
foreach ($required in @(
  "tag=v00.00.04",
  "asset=FluentPinyin.msi",
  "url=https://example.invalid/downloads/FluentPinyin.msi?label=FluentPinyin%20MSI"
)) {
  if ($parseOutput -notmatch [regex]::Escape($required)) {
    throw "Release JSON parser output missing '$required'"
  }
}
Remove-Item -LiteralPath $releaseJson -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $parseOutputFile -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $parseErrorFile -Force -ErrorAction SilentlyContinue

Write-Host "Smoke test passed for FluentPinyin $ExpectedVersion"
