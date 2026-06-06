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

Write-Host "Smoke test passed for FluentPinyin $ExpectedVersion"
