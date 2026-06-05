param(
    [string]$SourceFontDir = ".\assets\fonts"
)

$ErrorActionPreference = "Stop"

function Remove-StalePendingDeletes {
    $sessionManager = "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager"
    $property = Get-ItemProperty `
        -Path $sessionManager `
        -Name PendingFileRenameOperations `
        -ErrorAction SilentlyContinue
    if (-not $property -or -not $property.PendingFileRenameOperations) {
        return
    }

    $needles = @(
        "\FluentPinyin",
        "MiSans-Regular.ttf",
        "MiSans-Medium.ttf",
        "MiSans-Semibold.ttf",
        "MiSansTC-Regular.ttf",
        "MiSansTC-Medium.ttf",
        "MiSansTC-Semibold.ttf",
        "MiSansL3-Regular.ttf",
        "SourceHanSansSC-Regular.otf",
        "SourceHanSansTC-Regular.otf",
        "PlangothicP1-Regular.ttf",
        "PlangothicP2-Regular.ttf"
    )

    $entries = [string[]]$property.PendingFileRenameOperations
    $filtered = @()
    for ($index = 0; $index -lt $entries.Count; $index += 2) {
        $source = $entries[$index]
        $target = if ($index + 1 -lt $entries.Count) { $entries[$index + 1] } else { "" }
        $remove = $false
        foreach ($needle in $needles) {
            if ($source.IndexOf($needle, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or
                $target.IndexOf($needle, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
                $remove = $true
                break
            }
        }
        if (-not $remove) {
            $filtered += $source
            if ($index + 1 -lt $entries.Count) {
                $filtered += $target
            }
        }
    }

    if ($filtered.Count -eq $entries.Count) {
        return
    }

    if ($filtered.Count -gt 0) {
        Set-ItemProperty `
            -Path $sessionManager `
            -Name PendingFileRenameOperations `
            -Value ([string[]]$filtered) `
            -Type MultiString `
            -ErrorAction SilentlyContinue
    } else {
        Remove-ItemProperty `
            -Path $sessionManager `
            -Name PendingFileRenameOperations `
            -ErrorAction SilentlyContinue
    }
}

Remove-StalePendingDeletes

$root = Split-Path -Parent $PSScriptRoot
if (-not [IO.Path]::IsPathRooted($SourceFontDir)) {
    $cwdRelative = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($SourceFontDir)
    if (Test-Path -LiteralPath $cwdRelative) {
        $SourceFontDir = $cwdRelative
    } else {
        $SourceFontDir = Join-Path $root $SourceFontDir
    }
}

if (-not (Test-Path -LiteralPath $SourceFontDir)) {
    throw "Cannot find font directory: $SourceFontDir"
}

$fonts = @(
    @{ File = "MiSans-Regular.ttf"; Name = "MiSans"; Install = $true },
    @{ File = "MiSans-Medium.ttf"; Name = "MiSans Medium"; Install = $true },
    @{ File = "MiSans-Semibold.ttf"; Name = "MiSans Semibold"; Install = $true },
    @{ File = "MiSansTC-Regular.ttf"; Name = "MiSans TC"; Install = $true },
    @{ File = "MiSansTC-Medium.ttf"; Name = "MiSans TC Medium"; Install = $true },
    @{ File = "MiSansTC-Semibold.ttf"; Name = "MiSans TC Semibold"; Install = $true },
    @{ File = "MiSansL3-Regular.ttf"; Name = "MiSans L3"; Install = $true },
    @{ File = "SourceHanSansSC-Regular.otf"; Name = "Source Han Sans SC"; Install = $true },
    @{ File = "SourceHanSansTC-Regular.otf"; Name = "Source Han Sans TC"; Install = $true },
    @{ File = "PlangothicP1-Regular.ttf"; Name = "Plangothic P1"; Install = $true },
    @{ File = "PlangothicP2-Regular.ttf"; Name = "Plangothic P2"; Install = $true }
)

$userFontDir = Join-Path $env:LOCALAPPDATA "Microsoft\Windows\Fonts"
New-Item -ItemType Directory -Force -Path $userFontDir | Out-Null
$fontRegistry = "HKCU:\Software\Microsoft\Windows NT\CurrentVersion\Fonts"
New-Item -Path $fontRegistry -Force | Out-Null

Add-Type -Namespace Win32 -Name FontApi -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("gdi32.dll", CharSet=System.Runtime.InteropServices.CharSet.Unicode)]
public static extern int AddFontResourceEx(string lpszFilename, uint fl, System.IntPtr pdv);
[System.Runtime.InteropServices.DllImport("user32.dll", CharSet=System.Runtime.InteropServices.CharSet.Unicode)]
public static extern System.IntPtr SendMessage(System.IntPtr hWnd, uint Msg, System.IntPtr wParam, System.IntPtr lParam);
'@ -ErrorAction SilentlyContinue

foreach ($font in $fonts) {
    $source = Join-Path $SourceFontDir $font.File
    if (-not (Test-Path -LiteralPath $source)) {
        if ($font.Install) {
            throw "Missing font: $source"
        }
        continue
    }

    if (-not $font.Install) {
        continue
    }

    $target = Join-Path $userFontDir $font.File
    try {
        Copy-Item -LiteralPath $source -Destination $target -Force
    } catch {
        if (-not (Test-Path -LiteralPath $target)) {
            throw
        }
        Write-Host "Using existing font file: $target"
    }
    New-ItemProperty `
        -Path $fontRegistry `
        -Name "$($font.Name) (TrueType)" `
        -Value $target `
        -PropertyType String `
        -Force | Out-Null
    [Win32.FontApi]::AddFontResourceEx($target, 0, [IntPtr]::Zero) | Out-Null
}

[Win32.FontApi]::SendMessage([IntPtr]0xffff, 0x001D, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Write-Host "MiSans, Source Han Sans, and Plangothic fonts installed."
