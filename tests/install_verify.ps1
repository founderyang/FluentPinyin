param(
  [string]$InstallDir = "C:\Program Files\FluentPinyin",
  [string]$ExpectedVersion = "00.00.04",
  [string]$ExpectedProductCode = "",
  [switch]$RequireWanxiangPatch
)

$ErrorActionPreference = "Stop"

function Assert-True {
  param(
    [bool]$Condition,
    [string]$Message
  )
  if (-not $Condition) {
    throw $Message
  }
}

function Get-FluentPinyinUninstallEntry {
  $localizedName = -join ([char]0x6D41, [char]0x7545, [char]0x62FC, [char]0x97F3)
  $roots = @(
    "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall",
    "HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall"
  )
  foreach ($root in $roots) {
    if (-not (Test-Path -LiteralPath $root)) {
      continue
    }
    foreach ($key in Get-ChildItem -LiteralPath $root) {
      $props = Get-ItemProperty -LiteralPath $key.PSPath -ErrorAction SilentlyContinue
      if ($props.DisplayName -eq "FluentPinyin" -or $props.DisplayName -eq $localizedName) {
        [PSCustomObject]@{
          Key = $key.PSChildName
          DisplayName = $props.DisplayName
          DisplayVersion = $props.DisplayVersion
          InstallLocation = $props.InstallLocation
          UninstallString = $props.UninstallString
        }
      }
    }
  }
}

$entries = @(Get-FluentPinyinUninstallEntry)
Assert-True ($entries.Count -eq 1) "Expected one FluentPinyin uninstall entry, found $($entries.Count)"
$entry = $entries[0]
Assert-True ($entry.DisplayVersion -eq $ExpectedVersion) "Expected version $ExpectedVersion, found $($entry.DisplayVersion)"
if ($ExpectedProductCode) {
  Assert-True ($entry.Key -eq $ExpectedProductCode) "Expected ProductCode $ExpectedProductCode, found $($entry.Key)"
}

Assert-True (Test-Path -LiteralPath $InstallDir -PathType Container) "Missing install directory: $InstallDir"
foreach ($file in @(
  "fluent-pinyin-core.dll",
  "fluent-pinyin-tsf.dll",
  "fluent-pinyin-settings.exe",
  "fluent-pinyin-updater.exe",
  "fluent-pinyin-devtools.exe",
  "README.txt"
)) {
  Assert-True (Test-Path -LiteralPath (Join-Path $InstallDir $file) -PathType Leaf) "Missing installed file: $file"
}

$readme = Get-Content -LiteralPath (Join-Path $InstallDir "README.txt") -Raw -Encoding UTF8
$localizedName = -join ([char]0x6D41, [char]0x7545, [char]0x62FC, [char]0x97F3)
$localizedInstallPath = -join ([char]0x9ED8, [char]0x8BA4, [char]0x5B89, [char]0x88C5, [char]0x8DEF, [char]0x5F84)
Assert-True ($readme -match "$localizedName $([regex]::Escape($ExpectedVersion))") "Installed README is not localized or has the wrong version"
Assert-True ($readme -match $localizedInstallPath) "Installed README is missing localized install path text"

$fontProps = Get-ItemProperty "HKLM:\Software\Microsoft\Windows NT\CurrentVersion\Fonts" -ErrorAction SilentlyContinue
foreach ($pattern in @("MiSans", "SourceHan", "Plangothic")) {
  $matches = @($fontProps.PSObject.Properties | Where-Object { $_.Name -like "*$pattern*" })
  Assert-True ($matches.Count -eq 0) "Unexpected system font registry entry matching $pattern"
}

$devtools = Join-Path $InstallDir "fluent-pinyin-devtools.exe"
$runtime = Start-Process -FilePath $devtools -ArgumentList "ensure-winapp-runtime" -Wait -PassThru -WindowStyle Hidden
Assert-True ($runtime.ExitCode -eq 0) "ensure-winapp-runtime failed with exit code $($runtime.ExitCode)"
$smoke = Start-Process -FilePath $devtools -ArgumentList "smoke" -Wait -PassThru -WindowStyle Hidden
Assert-True ($smoke.ExitCode -eq 0) "devtools smoke failed with exit code $($smoke.ExitCode)"

if ($RequireWanxiangPatch) {
  $rimeDir = Join-Path $env:APPDATA "FluentPinyin\Rime"
  $basePatch = Join-Path $rimeDir "wanxiang.custom.yaml"
  $proPatch = Join-Path $rimeDir "wanxiang_pro.custom.yaml"
  Assert-True (Test-Path -LiteralPath $basePatch -PathType Leaf) "Missing Wanxiang base custom patch"
  Assert-True (Test-Path -LiteralPath $proPatch -PathType Leaf) "Missing Wanxiang pro custom patch"
  $baseText = Get-Content -LiteralPath $basePatch -Raw -Encoding UTF8
  $proText = Get-Content -LiteralPath $proPatch -Raw -Encoding UTF8
  Assert-True ($baseText -match "translator/enable_user_dict:\s*true") "Wanxiang base translator tuning is not enabled"
  Assert-True ($proText -match "translator/enable_user_dict:\s*false") "Wanxiang pro translator tuning should remain disabled"
}

Write-Host "Install verification passed for FluentPinyin $ExpectedVersion"
