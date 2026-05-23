param(
    [string]$BuildDir = ".\build-input-hardening",
    [string]$Config = "Debug",
    [switch]$OnlineUpdateCheck
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $binDir = Join-Path (Resolve-Path -Path $BuildDir) "bin\$Config"
    $smoke = Join-Path $binDir "fp-rime-smoke.exe"
    $importer = Join-Path $binDir "fp-lexicon-import.exe"
    $updater = Join-Path $binDir "fp-updater.exe"

    foreach ($tool in @($smoke, $importer, $updater)) {
        if (-not (Test-Path -LiteralPath $tool)) {
            throw "Missing test tool: $tool"
        }
    }

    & $smoke nihao
    if ($LASTEXITCODE -ne 0) {
        throw "RIME smoke failed with exit code $LASTEXITCODE"
    }

    $testDir = Join-Path $env:TEMP "FluentPinyinTestDev"
    Remove-Item -LiteralPath $testDir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $testDir | Out-Null
    $dict = Join-Path $testDir "fp_test_dev.dict.yaml"
    @'
---
name: fp_test_dev
version: "1"
sort: by_weight
...

流畅拼音	liu chang pin yin	100
'@ | Set-Content -LiteralPath $dict -Encoding utf8

    & $importer $dict $testDir
    if ($LASTEXITCODE -ne 0) {
        throw "Lexicon import failed with exit code $LASTEXITCODE"
    }

    $aggregate = Join-Path $testDir "fp_frost.dict.yaml"
    $aggregateText = Get-Content -LiteralPath $aggregate -Raw
    if ($aggregateText -notmatch "fp_test_dev") {
        throw "Aggregate dictionary did not include imported table."
    }

    if ($OnlineUpdateCheck) {
        & $updater check
        if ($LASTEXITCODE -ne 0) {
            throw "Update check failed with exit code $LASTEXITCODE"
        }
    }

    $defaultTip = Get-WinDefaultInputMethodOverride
    if ($defaultTip -and $defaultTip.InputMethodTip -notmatch "81D4E9C9") {
        Write-Warning "Default input method is not Microsoft Pinyin: $($defaultTip.InputMethodTip)"
    }

    Write-Host "FluentPinyin dev tests passed."
} finally {
    Pop-Location
}
