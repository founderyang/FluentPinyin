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

function Invoke-ReleaseJsonParser {
  param(
    [string]$Updater,
    [string]$JsonText,
    [int]$ExpectedExitCode,
    [string[]]$ExpectedOutput = @(),
    [switch]$Utf8Bom
  )

  $suffix = [Guid]::NewGuid().ToString("N")
  $releaseJson = Join-Path $env:TEMP "fluent-pinyin-release-$suffix.json"
  $parseOutputFile = Join-Path $env:TEMP "fluent-pinyin-release-$suffix.out"
  $parseErrorFile = Join-Path $env:TEMP "fluent-pinyin-release-$suffix.err"
  try {
    if ($Utf8Bom) {
      $encoding = New-Object System.Text.UTF8Encoding($true)
      [System.IO.File]::WriteAllText($releaseJson, $JsonText, $encoding)
    } else {
      Set-Content -LiteralPath $releaseJson -Encoding UTF8 -Value $JsonText
    }

    $parseProcess = Start-Process -FilePath $Updater `
      -ArgumentList @("parse-release-json", $releaseJson, "FluentPinyin.msi") `
      -Wait `
      -PassThru `
      -WindowStyle Hidden `
      -RedirectStandardOutput $parseOutputFile `
      -RedirectStandardError $parseErrorFile
    if ($parseProcess.ExitCode -ne $ExpectedExitCode) {
      $parseError = ""
      if (Test-Path -LiteralPath $parseErrorFile) {
        $parseError = Get-Content -LiteralPath $parseErrorFile -Raw
      }
      throw "Release JSON parser expected exit code $ExpectedExitCode, got $($parseProcess.ExitCode): $parseError"
    }

    $parseOutput = ""
    if (Test-Path -LiteralPath $parseOutputFile) {
      $parseOutput = Get-Content -LiteralPath $parseOutputFile -Raw
    }
    foreach ($required in $ExpectedOutput) {
      if ($parseOutput -notmatch [regex]::Escape($required)) {
        throw "Release JSON parser output missing '$required'"
      }
    }
  } finally {
    Remove-Item -LiteralPath $releaseJson -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $parseOutputFile -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $parseErrorFile -Force -ErrorAction SilentlyContinue
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

Invoke-ReleaseJsonParser -Updater $updater -ExpectedExitCode 0 -ExpectedOutput @(
  "tag=v00.00.04",
  "asset=FluentPinyin.msi",
  "url=https://example.invalid/downloads/FluentPinyin.msi?label=FluentPinyin%20MSI"
) -JsonText @'
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
'@

Invoke-ReleaseJsonParser -Updater $updater -ExpectedExitCode 0 -Utf8Bom -ExpectedOutput @(
  "tag=v00.00.04",
  "asset=FluentPinyin.msi",
  "url=https://example.invalid/releases/FluentPinyin.msi?x=quote%5C%22"
) -JsonText @'
{
  "tag_name": "v00.00.04",
  "assets": [
    {
      "name": "FluentPinyin.msi",
      "browser_download_url": "https://example.invalid/releases/FluentPinyin.msi?x=quote%5C%22"
    }
  ]
}
'@

Invoke-ReleaseJsonParser -Updater $updater -ExpectedExitCode 2 -JsonText @'
{
  "tag_name": "v00.00.04",
  "assets": [
    {
      "name": "FluentPinyin.zip",
      "browser_download_url": "https://example.invalid/FluentPinyin.zip"
    }
  ]
}
'@

Write-Host "Smoke test passed for FluentPinyin $ExpectedVersion"
