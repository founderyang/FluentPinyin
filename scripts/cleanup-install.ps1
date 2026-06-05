param(
    [string]$InstallDir = "$env:ProgramFiles\FluentPinyin",
    [switch]$SkipInstallDir,
    [switch]$KeepUserData
)

$ErrorActionPreference = "Continue"

function Remove-RegistryTree([string]$Path) {
    Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction SilentlyContinue
}

function Remove-RegistryValue([string]$Path, [string]$Name) {
    Remove-ItemProperty -Path $Path -Name $Name -ErrorAction SilentlyContinue
}

function Remove-RegistryValuesMatching(
    [string]$Path,
    [string[]]$NameContains,
    [string[]]$ValueContains
) {
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $item = Get-ItemProperty -LiteralPath $Path -ErrorAction SilentlyContinue
    if (-not $item) {
        return
    }

    foreach ($property in $item.PSObject.Properties) {
        if ($property.Name -like "PS*") {
            continue
        }

        $name = [string]$property.Name
        $value = [string]$property.Value
        $matched = $false
        foreach ($needle in $NameContains) {
            if (-not [string]::IsNullOrWhiteSpace($needle) -and
                $name.IndexOf($needle, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
                $matched = $true
                break
            }
        }
        if (-not $matched) {
            foreach ($needle in $ValueContains) {
                if (-not [string]::IsNullOrWhiteSpace($needle) -and
                    $value.IndexOf($needle, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
                    $matched = $true
                    break
                }
            }
        }

        if ($matched) {
            Remove-RegistryValue -Path $Path -Name $property.Name
        }
    }
}

$fonts = @(
    @{ File = "MiSans-Regular.ttf"; Name = "MiSans" },
    @{ File = "MiSans-Medium.ttf"; Name = "MiSans Medium" },
    @{ File = "MiSans-Semibold.ttf"; Name = "MiSans Semibold" },
    @{ File = "MiSansTC-Regular.ttf"; Name = "MiSans TC" },
    @{ File = "MiSansTC-Medium.ttf"; Name = "MiSans TC Medium" },
    @{ File = "MiSansTC-Semibold.ttf"; Name = "MiSans TC Semibold" },
    @{ File = "MiSansL3-Regular.ttf"; Name = "MiSans L3" },
    @{ File = "SourceHanSansSC-Regular.otf"; Name = "Source Han Sans SC" },
    @{ File = "SourceHanSansTC-Regular.otf"; Name = "Source Han Sans TC" },
    @{ File = "PlangothicP1-Regular.ttf"; Name = "Plangothic P1" },
    @{ File = "PlangothicP2-Regular.ttf"; Name = "Plangothic P2" }
)

Add-Type -Namespace Win32 -Name FontApi -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("gdi32.dll", CharSet=System.Runtime.InteropServices.CharSet.Unicode)]
public static extern bool RemoveFontResourceEx(string lpszFilename, uint fl, System.IntPtr pdv);
[System.Runtime.InteropServices.DllImport("user32.dll", CharSet=System.Runtime.InteropServices.CharSet.Unicode)]
public static extern System.IntPtr SendMessage(System.IntPtr hWnd, uint Msg, System.IntPtr wParam, System.IntPtr lParam);
'@ -ErrorAction SilentlyContinue

Add-Type -Namespace Win32 -Name InstallCleanup -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet=System.Runtime.InteropServices.CharSet.Unicode, SetLastError=true)]
public static extern bool MoveFileEx(string lpExistingFileName, System.IntPtr lpNewFileName, int dwFlags);
'@ -ErrorAction SilentlyContinue

function Register-DeleteOnReboot([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        return
    }

    $resolved = $Path
    try {
        $resolved = (Resolve-Path -LiteralPath $Path -ErrorAction Stop).ProviderPath
    } catch {
    }

    [Win32.InstallCleanup]::MoveFileEx($resolved, [IntPtr]::Zero, 0x4) | Out-Null
}

function Remove-PathTree([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        return
    }

    try {
        Get-ChildItem -LiteralPath $Path -Force -Recurse -ErrorAction SilentlyContinue |
            ForEach-Object {
                try {
                    $_.Attributes = "Normal"
                } catch {
                }
            }
        Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
        return
    } catch {
    }

    $remaining = @(Get-ChildItem -LiteralPath $Path -Force -Recurse -ErrorAction SilentlyContinue |
        Sort-Object { $_.FullName.Length } -Descending)
    foreach ($item in $remaining) {
        try {
            Remove-Item -LiteralPath $item.FullName -Recurse -Force -ErrorAction Stop
        } catch {
            Register-DeleteOnReboot -Path $item.FullName
        }
    }

    try {
        Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
    } catch {
        Register-DeleteOnReboot -Path $Path
    }
}

$userFontDir = Join-Path $env:LOCALAPPDATA "Microsoft\Windows\Fonts"
$fontRegistry = "HKCU:\Software\Microsoft\Windows NT\CurrentVersion\Fonts"
foreach ($font in $fonts) {
    $target = Join-Path $userFontDir $font.File
    if (Test-Path -LiteralPath $target) {
        [Win32.FontApi]::RemoveFontResourceEx($target, 0, [IntPtr]::Zero) | Out-Null
    }
    Remove-RegistryValue -Path $fontRegistry -Name "$($font.Name) (TrueType)"
    Remove-RegistryValue -Path $fontRegistry -Name "$($font.Name) (OpenType)"
    try {
        Remove-Item -LiteralPath $target -Force -ErrorAction Stop
    } catch {
        Register-DeleteOnReboot -Path $target
    }
}
[Win32.FontApi]::SendMessage([IntPtr]0xffff, 0x001D, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null

schtasks.exe /Delete /TN "\FluentPinyinAutoSync" /F 2>$null | Out-Null
schtasks.exe /Delete /TN "FluentPinyinAutoSync" /F 2>$null | Out-Null

$clsid = "{76e3ad5b-1dd8-4584-b3cd-127df0239720}"
$profile = "{21e29f6d-32dc-4f6d-8477-9ed72313c625}"
$userProfileValue = "0804:$clsid$profile"
$keyboardLayout = "E0200804"

$registryKeys = @(
    "Registry::HKEY_LOCAL_MACHINE\Software\FluentPinyin",
    "Registry::HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Uninstall\FluentPinyin",
    "Registry::HKEY_LOCAL_MACHINE\Software\Microsoft\CTF\TIP\$clsid",
    "Registry::HKEY_CURRENT_USER\Software\Microsoft\CTF\TIP\$clsid",
    "Registry::HKEY_CURRENT_USER\Software\Classes\CLSID\$clsid",
    "Registry::HKEY_LOCAL_MACHINE\Software\Classes\CLSID\$clsid",
    "Registry::HKEY_CLASSES_ROOT\CLSID\$clsid",
    "Registry::HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Keyboard Layouts\$keyboardLayout"
)
foreach ($key in $registryKeys) {
    Remove-RegistryTree -Path $key
}

Remove-RegistryValue -Path "HKCU:\Control Panel\International\User Profile\zh-Hans-CN" -Name $userProfileValue

$installDirNormalized = $InstallDir.TrimEnd('\')
Remove-RegistryValuesMatching `
    -Path "HKCU:\Software\Classes\Local Settings\Software\Microsoft\Windows\Shell\MuiCache" `
    -NameContains @($installDirNormalized, "FluentPinyin", "fluent-pinyin") `
    -ValueContains @($installDirNormalized, "FluentPinyin", "fluent-pinyin")
Remove-RegistryValuesMatching `
    -Path "HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Compatibility Assistant\Store" `
    -NameContains @($installDirNormalized, "FluentPinyin", "fluent-pinyin") `
    -ValueContains @($installDirNormalized, "FluentPinyin", "fluent-pinyin")

if (-not $KeepUserData) {
    foreach ($path in @(
        (Join-Path $env:APPDATA "FluentPinyin"),
        (Join-Path $env:LOCALAPPDATA "FluentPinyin"),
        (Join-Path $env:ProgramData "FluentPinyin"),
        (Join-Path $env:TEMP "FluentPinyin-update")
    )) {
        Remove-PathTree -Path $path
    }
}

if (-not $SkipInstallDir) {
    Remove-PathTree -Path $InstallDir
}
