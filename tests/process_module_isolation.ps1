param(
  [string[]]$AllowedCoreProcesses = @(
    "fluent-pinyin-corehost",
    "fluent-pinyin-devtools",
    "corehost_runtime_smoke",
    "core_unit"
  )
)

$ErrorActionPreference = "Stop"

$matches = @()
Get-Process | ForEach-Object {
  $process = $_
  try {
    foreach ($module in $process.Modules) {
      if ($module.FileName -like "*FluentPinyin*" -or
          $module.FileName -like "*fluent-pinyin*" -or
          $module.FileName -like "*\rime.dll") {
        $matches += [PSCustomObject]@{
          Id = $process.Id
          ProcessName = $process.ProcessName
          Module = Split-Path -Leaf $module.FileName
          Path = $module.FileName
        }
      }
    }
  } catch {
  }
}

$bad = $matches | Where-Object {
  $_.ProcessName -notin $AllowedCoreProcesses -and
  ($_.Module -in @("fluent-pinyin-core.dll", "rime.dll") -or $_.Module -like "opencc*")
}

if ($bad) {
  $details = ($bad | ForEach-Object { "$($_.ProcessName):$($_.Module)" }) -join ", "
  throw "Host process loaded core/Rime unexpectedly: $details"
}

if ($matches.Count -gt 0) {
  $matches | Sort-Object ProcessName,Module | Format-Table -AutoSize
}
Write-Host "Process module isolation passed: host processes do not load fluent-pinyin-core.dll or rime.dll."
