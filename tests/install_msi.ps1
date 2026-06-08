param(
  [Parameter(Mandatory = $true)]
  [string]$MsiPath,

  [Parameter(Mandatory = $true)]
  [string]$LogPath,

  [int]$TimeoutSeconds = 900
)

$ErrorActionPreference = "Stop"

function Quote-ProcessArgument {
  param([string]$Value)
  if ($Value -notmatch '[\s"]') {
    return $Value
  }
  return '"' + ($Value -replace '"', '\"') + '"'
}

function Read-LogTail {
  param(
    [string]$Path,
    [int]$Count = 120
  )
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    return ""
  }
  return (Get-Content -LiteralPath $Path -Tail $Count -Encoding UTF8) -join [Environment]::NewLine
}

function Test-InstallSucceeded {
  param([string]$Path)
  $tail = Read-LogTail -Path $Path -Count 160
  return $tail -match "MainEngineThread is returning 0" -or
         $tail -match "安装成功或错误状态:\s*0"
}

function Assert-NoRestartPromptMarkers {
  param([string]$Path)
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    return
  }
  $text = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
  foreach ($marker in @(
    "ScheduleReboot",
    "ForceReboot",
    "PendingFileRenameOperations",
    "REBOOTPROMPT",
    "MsiRMFilesInUse",
    "FilesInUse"
  )) {
    if ($text -match [regex]::Escape($marker)) {
      throw "MSI log contains restart prompt marker '$marker': $Path"
    }
  }
}

$resolvedMsi = (Resolve-Path -LiteralPath $MsiPath).Path
$resolvedLog = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($LogPath)
$logDir = Split-Path -Parent $resolvedLog
if ($logDir) {
  New-Item -ItemType Directory -Path $logDir -Force | Out-Null
}
Remove-Item -LiteralPath $resolvedLog -Force -ErrorAction SilentlyContinue

$msiexec = Join-Path $env:WINDIR "System32\msiexec.exe"
$arguments = @(
  "/i",
  (Quote-ProcessArgument $resolvedMsi),
  "/qn",
  "/norestart",
  "/l*v",
  (Quote-ProcessArgument $resolvedLog)
) -join " "

Write-Host "msiexec.exe $arguments"
$process = Start-Process -FilePath $msiexec -ArgumentList $arguments -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds($TimeoutSeconds)

while ((Get-Date) -lt $deadline) {
  Start-Sleep -Seconds 2
  $running = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
  if (-not $running) {
    if (Test-InstallSucceeded -Path $resolvedLog) {
      Assert-NoRestartPromptMarkers -Path $resolvedLog
      Write-Host "MSI install completed successfully"
      exit 0
    }
    $process.Refresh()
    $exitCode = $process.ExitCode
    $tail = Read-LogTail -Path $resolvedLog
    throw "MSI install process exited with code $exitCode.`n$tail"
  }
  if (Test-InstallSucceeded -Path $resolvedLog) {
    Assert-NoRestartPromptMarkers -Path $resolvedLog
    Write-Host "MSI install completed successfully"
    exit 0
  }
}

$tail = Read-LogTail -Path $resolvedLog
throw "MSI install timed out after $TimeoutSeconds seconds. msiexec PID: $($process.Id).`n$tail"
