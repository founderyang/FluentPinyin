param(
  [Parameter(Mandatory = $true)]
  [string]$TsfDll,

  [string]$SourceRoot = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
  $scriptDir = if (-not [string]::IsNullOrWhiteSpace($PSScriptRoot)) {
    $PSScriptRoot
  } else {
    Split-Path -Parent $PSCommandPath
  }
  $SourceRoot = (Resolve-Path -LiteralPath (Join-Path $scriptDir "..")).Path
}

if (-not (Test-Path -LiteralPath $TsfDll -PathType Leaf)) {
  throw "Missing TSF DLL: $TsfDll"
}

$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $TsfDll))
$ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
$unicode = [System.Text.Encoding]::Unicode.GetString($bytes)

if ($ascii -match "fluent-pinyin-core\.dll" -or $unicode -match "fluent-pinyin-core\.dll") {
  throw "TSF must not import or directly reference fluent-pinyin-core.dll; core must stay in fluent-pinyin-corehost.exe for QQ/TIM compatibility."
}

$guidsHeader = Join-Path $SourceRoot "src\tsf\guids.h"
$textServiceSource = Join-Path $SourceRoot "src\tsf\tsf_text_service.cpp"
$textServiceHeader = Join-Path $SourceRoot "src\tsf\tsf_text_service.h"
foreach ($sourceFile in @($guidsHeader, $textServiceSource, $textServiceHeader)) {
  if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) {
    throw "Missing TSF source file for language-bar verification: $sourceFile"
  }
}
$guidsText = Get-Content -LiteralPath $guidsHeader -Raw -Encoding UTF8
$sourceText = Get-Content -LiteralPath $textServiceSource -Raw -Encoding UTF8
$headerText = Get-Content -LiteralPath $textServiceHeader -Raw -Encoding UTF8
foreach ($required in @(
  "kBrandLangBarItemGuid",
  "kInputModeLangBarItemGuid"
)) {
  if ($guidsText -notmatch [regex]::Escape($required)) {
    throw "TSF language bar is missing required GUID: $required"
  }
}
foreach ($required in @(
  "brand_item_",
  "input_mode_item_"
)) {
  if ($headerText -notmatch [regex]::Escape($required)) {
    throw "TSF language bar must keep separate brand and input-mode item members: $required"
  }
}
foreach ($required in @(
  "LangBarItemKind::kBrand",
  "LangBarItemKind::kInputMode",
  "kBrandLangBarItemGuid",
  "kInputModeLangBarItemGuid",
  "AddItem(brand_item_)",
  "AddItem(input_mode_item_)"
)) {
  if ($sourceText -notmatch [regex]::Escape($required)) {
    throw "TSF language bar must keep the tray brand logo and 中/英 input-state item side by side. Missing source marker: $required"
  }
}

$nativeSource = @"
using System;
using System.Runtime.InteropServices;

public static class FluentPinyinTsfNative {
  [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  public static extern IntPtr LoadLibraryExW(string lpFileName, IntPtr hReservedNull, int dwFlags);

  [DllImport("kernel32.dll", SetLastError = true)]
  public static extern IntPtr FindResourceW(IntPtr hModule, IntPtr lpName, IntPtr lpType);

  [DllImport("kernel32.dll", SetLastError = true)]
  public static extern bool FreeLibrary(IntPtr hModule);
}
"@
Add-Type -TypeDefinition $nativeSource -ErrorAction SilentlyContinue | Out-Null

$LOAD_LIBRARY_AS_DATAFILE = 0x00000002
$RT_GROUP_ICON = [IntPtr]14
$module = [FluentPinyinTsfNative]::LoadLibraryExW((Resolve-Path -LiteralPath $TsfDll), [IntPtr]::Zero, $LOAD_LIBRARY_AS_DATAFILE)
if ($module -eq [IntPtr]::Zero) {
  throw "Unable to inspect TSF icon resources: LoadLibraryEx failed."
}
try {
  foreach ($resourceId in @(1, 2, 3)) {
    $resource = [FluentPinyinTsfNative]::FindResourceW($module, [IntPtr]$resourceId, $RT_GROUP_ICON)
    if ($resource -eq [IntPtr]::Zero) {
      throw "TSF missing icon resource id $resourceId; the tray brand logo must stay embedded."
    }
  }
} finally {
  [void][FluentPinyinTsfNative]::FreeLibrary($module)
}

Write-Host "TSF dependency verification passed"
