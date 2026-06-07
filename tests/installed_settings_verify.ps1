param(
  [string]$SettingsPath = (Join-Path $env:APPDATA "FluentPinyin\settings.ini"),
  [string]$SyncSettingsExe = ""
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

$settingsDir = Split-Path -Parent $SettingsPath
New-Item -ItemType Directory -Force -Path $settingsDir | Out-Null
$backupPath = Join-Path $settingsDir ("settings.ini.codex-backup-" + [Guid]::NewGuid().ToString("N"))
if (Test-Path -LiteralPath $SettingsPath -PathType Leaf) {
  Copy-Item -LiteralPath $SettingsPath -Destination $backupPath -Force
} else {
  Set-Content -LiteralPath $backupPath -Encoding UTF8 -Value ""
}

try {
  $testSettings = @(
    "settings_schema_version=1",
    "default_input_mode=en",
    "default_charset=traditional",
    "default_shape_half=0",
    "default_chinese_punctuation=0",
    "candidate_count=7",
    "candidate_font_size_level=8",
    "candidate_font_family=Microsoft YaHei UI",
    "candidate_layout=vertical",
    "candidate_horizontal=0",
    "toolbar_visible_v2=1",
    "toolbar_visible=0",
    "toolbar_layout=vertical",
    "toolbar_items=input_mode,shape,punctuation,charset,emoji",
    "status_tip_enabled=1",
    "status_tip_blacklist=QQ.exe,WeChat.exe",
    "input_scheme=double_pinyin",
    "double_pinyin_scheme=zrm",
    "auto_pinyin_correction=0",
    "super_abbrev=0",
    "smart_fuzzy_pinyin=0",
    "fuzzy_pinyin=1",
    "fuzzy_pinyin_rules=nl,ry",
    "fuzzy_pinyin_custom_rules=foo=bar",
    "user_lexicon_enabled=0",
    "custom_phrases_enabled=0",
    "imported_lexicons_enabled=0",
    "name_input=0",
    "u_mode=0",
    "v_mode=0",
    "shortcut_candidate_expand=Ctrl+Space",
    "shortcut_candidate_previous_page=PageUp",
    "shortcut_candidate_next_page=PageDown",
    "sync_provider=webdav",
    "sync_clipboard=1",
    "sync_user_data=1",
    "sync_auto_enabled=1",
    "sync_auto_interval_minutes=15",
    "sync_webdav_url=https://example.invalid/dav",
    "sync_webdav_username=codex-test",
    "sync_encryption_secret=codex-secret"
  )
  Set-Content -LiteralPath $SettingsPath -Encoding UTF8 -Value $testSettings
  $read = Get-Content -LiteralPath $SettingsPath -Encoding UTF8
  foreach ($item in @(
    "default_input_mode=en",
    "default_charset=traditional",
    "candidate_count=7",
    "candidate_layout=vertical",
    "toolbar_visible_v2=1",
    "input_scheme=double_pinyin",
    "fuzzy_pinyin=1",
    "sync_provider=webdav",
    "sync_auto_interval_minutes=15"
  )) {
    Assert-True ($read -contains $item) "Missing setting after write: $item"
  }

  if ($SyncSettingsExe) {
    Assert-True (Test-Path -LiteralPath $SyncSettingsExe -PathType Leaf) "Missing sync settings verifier: $SyncSettingsExe"
    $workDir = Join-Path ([System.IO.Path]::GetTempPath()) ("FluentPinyin-installed-sync-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $workDir | Out-Null
    try {
      $process = Start-Process -FilePath $SyncSettingsExe `
        -ArgumentList $workDir `
        -Wait `
        -PassThru `
        -WindowStyle Hidden
      Assert-True ($process.ExitCode -eq 0) "sync settings verifier failed with exit code $($process.ExitCode)"
    } finally {
      Remove-Item -LiteralPath $workDir -Recurse -Force -ErrorAction SilentlyContinue
    }
  }

  Write-Host "Installed settings verification passed: $SettingsPath"
} finally {
  if (Test-Path -LiteralPath $backupPath -PathType Leaf) {
    Move-Item -LiteralPath $backupPath -Destination $SettingsPath -Force
  }
}
