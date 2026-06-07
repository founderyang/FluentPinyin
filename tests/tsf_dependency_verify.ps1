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
  "AddItem(input_mode_item_)",
  "CachedFluentPinyinBrandIcon(width, height)",
  "CreateTrayInputIcon(mode)",
  "UserLanguageProfileKeyName()",
  "UserLanguageProfileKey()",
  "InputMethodOverride",
  "KeyboardPreloadKey()",
  "SubstituteKeyboardLayoutId()"
)) {
  if ($sourceText -notmatch [regex]::Escape($required)) {
    throw "TSF language bar must keep the tray brand logo and 中/英 input-state item side by side. Missing source marker: $required"
  }
}

foreach ($requiredPattern in @(
  'kind_\s*==\s*LangBarItemKind::kBrand[\s\S]*info->guidItem\s*=\s*kBrandLangBarItemGuid',
  'kind_\s*==\s*LangBarItemKind::kBrand[\s\S]*CachedFluentPinyinBrandIcon\(width,\s*height\)',
  'else\s*\{[\s\S]*info->guidItem\s*=\s*kInputModeLangBarItemGuid',
  'else\s*\{[\s\S]*TF_LBI_STYLE_TEXTCOLORICON',
  'else\s*\{[\s\S]*CreateTrayInputIcon\(mode\)',
  'STDMETHODIMP GetText\(BSTR\* text\)[\s\S]*SysAllocString\(L"\\u7545"\)',
  'SetRegistryDwordIfChanged\(\s*HKEY_CURRENT_USER,\s*UserLanguageProfileKey\(\),\s*UserLanguageProfileKeyName\(\)\.c_str\(\),\s*1\)',
  'SetRegistryStringIfChanged\(\s*HKEY_CURRENT_USER,\s*UserProfileKey\(\),\s*L"InputMethodOverride",\s*UserLanguageProfileKeyName\(\)\)',
  'SetRegistryStringIfChanged\(\s*HKEY_CURRENT_USER,\s*KeyboardPreloadKey\(\),\s*L"1",\s*SubstituteKeyboardLayoutId\(\)\)'
)) {
  if ($sourceText -notmatch $requiredPattern) {
    throw "TSF language-bar behavior regression: missing required separated brand/input-mode implementation pattern."
  }
}

foreach ($forbiddenPattern in @(
  'kBrandLangBarItemGuid[\s\S]{0,300}TF_LBI_STYLE_TEXTCOLORICON',
  'LangBarItemKind::kBrand[\s\S]{0,500}CreateTrayInputIcon',
  'LangBarItemKind::kInputMode[\s\S]{0,500}CachedFluentPinyinBrandIcon'
)) {
  if ($sourceText -match $forbiddenPattern) {
    throw "TSF language bar mixed brand and input-state behavior; 畅 and 中/英 must remain separate side-by-side items."
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
