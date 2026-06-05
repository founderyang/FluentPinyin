param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$downloadsDir = Join-Path $root "packages\downloads"
$winuiDir = Join-Path $root "packages\winui"
$librimeDir = Join-Path $root "third_party\librime"
$wanxiangDir = Join-Path $root "schemas\wanxiang\current"
$fontDir = Join-Path $root "assets\fonts"
$assetsPath = Join-Path $downloadsDir "m2-assets.json"

New-Item -ItemType Directory -Force -Path $downloadsDir, $winuiDir, $librimeDir, $fontDir | Out-Null

$defaultAssets = @{
    webview2 = "microsoft.web.webview2.1.0.3179.45.nupkg"
    webview2Url = "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.3179.45"
    cppwinrt = "microsoft.windows.cppwinrt.3.0.260520.1.nupkg"
    cppwinrtUrl = "https://www.nuget.org/api/v2/package/Microsoft.Windows.CppWinRT/3.0.260520.1"
    windowsAppSdkFoundation = "microsoft.windowsappsdk.foundation.1.8.260505001.nupkg"
    windowsAppSdkFoundationUrl = "https://www.nuget.org/api/v2/package/Microsoft.WindowsAppSDK.Foundation/1.8.260505001"
    windowsAppSdkWinui = "microsoft.windowsappsdk.winui.1.8.260505002.nupkg"
    windowsAppSdkWinuiUrl = "https://www.nuget.org/api/v2/package/Microsoft.WindowsAppSDK.WinUI/1.8.260505002"
    windowsAppSdkInteractive = "microsoft.windowsappsdk.interactiveexperiences.1.8.260430001.nupkg"
    windowsAppSdkInteractiveUrl = "https://www.nuget.org/api/v2/package/Microsoft.WindowsAppSDK.InteractiveExperiences/1.8.260430001"
    librime = "rime-de4700e-Windows-msvc-x64.7z"
    librimeUrl = "https://github.com/rime/librime/releases/download/1.16.1/rime-de4700e-Windows-msvc-x64.7z"
    deps = "rime-deps-de4700e-Windows-msvc-x64.7z"
    depsUrl = "https://github.com/rime/librime/releases/download/1.16.1/rime-deps-de4700e-Windows-msvc-x64.7z"
    wanxiang = "rime-wanxiang-base.zip"
    wanxiangUrl = "https://github.com/amzxyz/rime-wanxiang/releases/download/v15.11.1/rime-wanxiang-base.zip"
    wanxiangGram = "wanxiang-lts-zh-hans.gram"
    wanxiangGramUrl = "https://github.com/amzxyz/RIME-LMDG/releases/download/LTS/wanxiang-lts-zh-hans.gram"
    miSans = "MiSans.zip"
    miSansUrl = "https://hyperos.mi.com/font-download/MiSans.zip"
    miSansTc = "MiSans_TC.zip"
    miSansTcUrl = "https://hyperos.mi.com/font-download/MiSans_TC.zip"
    miSansL3 = "MiSans_L3.zip"
    miSansL3Url = "https://hyperos.mi.com/font-download/MiSans_L3.zip"
    sourceHanSansSc = "SourceHanSansSC.zip"
    sourceHanSansScUrl = "https://github.com/adobe-fonts/source-han-sans/releases/download/2.005R/09_SourceHanSansSC.zip"
    sourceHanSansTc = "SourceHanSansTC.zip"
    sourceHanSansTcUrl = "https://github.com/adobe-fonts/source-han-sans/releases/download/2.005R/10_SourceHanSansTC.zip"
    plangothicP1 = "PlangothicP1-Regular.ttf"
    plangothicP1Url = "https://github.com/Fitzgerald-Porthmouth-Koenigsegg/Plangothic_Project/releases/download/V2.9.5792/PlangothicP1-Regular.ttf"
    plangothicP2 = "PlangothicP2-Regular.ttf"
    plangothicP2Url = "https://github.com/Fitzgerald-Porthmouth-Koenigsegg/Plangothic_Project/releases/download/V2.9.5792/PlangothicP2-Regular.ttf"
}

if (-not (Test-Path -LiteralPath $assetsPath)) {
    $defaultAssets | ConvertTo-Json | Set-Content -Path $assetsPath -Encoding UTF8
}

$assets = Get-Content -Path $assetsPath | ConvertFrom-Json
if (-not $assets.PSObject.Properties["webview2"] -or
    -not $assets.PSObject.Properties["cppwinrt"] -or
    -not $assets.PSObject.Properties["windowsAppSdkFoundation"] -or
    -not $assets.PSObject.Properties["windowsAppSdkWinui"] -or
    -not $assets.PSObject.Properties["windowsAppSdkInteractive"] -or
    -not $assets.PSObject.Properties["wanxiang"] -or
    -not $assets.PSObject.Properties["wanxiangUrl"] -or
    -not $assets.PSObject.Properties["wanxiangGram"] -or
    -not $assets.PSObject.Properties["wanxiangGramUrl"] -or
    -not $assets.PSObject.Properties["miSans"] -or
    -not $assets.PSObject.Properties["miSansUrl"] -or
    -not $assets.PSObject.Properties["miSansTc"] -or
    -not $assets.PSObject.Properties["miSansTcUrl"] -or
    -not $assets.PSObject.Properties["miSansL3"] -or
    -not $assets.PSObject.Properties["miSansL3Url"] -or
    -not $assets.PSObject.Properties["sourceHanSansSc"] -or
    -not $assets.PSObject.Properties["sourceHanSansTc"] -or
    -not $assets.PSObject.Properties["plangothicP1"] -or
    -not $assets.PSObject.Properties["plangothicP2"]) {
    $assets = [pscustomobject]$defaultAssets
    $defaultAssets | ConvertTo-Json | Set-Content -Path $assetsPath -Encoding UTF8
}

function Get-AssetFile([string]$Name, [string]$Url) {
    $archive = Join-Path $downloadsDir $Name
    if ($Force -or -not (Test-Path -LiteralPath $archive)) {
        Write-Host "Downloading $Url"
        Invoke-WebRequest -Uri $Url -OutFile $archive
    } else {
        Write-Host "Using cached $archive"
    }
    return $archive
}

function Expand-Package([string]$Archive, [string]$Destination) {
    if ($Force -and (Test-Path -LiteralPath $Destination)) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    if (Test-Path -LiteralPath $Destination) {
        Write-Host "Using extracted $Destination"
        return
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Write-Host "Extracting $Archive -> $Destination"
    tar -xf $Archive -C $Destination
}

$nugetPackages = @(
    @{ Name = $assets.webview2; Url = $assets.webview2Url; Dir = "microsoft.web.webview2.1.0.3179.45" },
    @{ Name = $assets.cppwinrt; Url = $assets.cppwinrtUrl; Dir = "microsoft.windows.cppwinrt.3.0.260520.1" },
    @{ Name = $assets.windowsAppSdkFoundation; Url = $assets.windowsAppSdkFoundationUrl; Dir = "microsoft.windowsappsdk.foundation.1.8.260505001" },
    @{ Name = $assets.windowsAppSdkWinui; Url = $assets.windowsAppSdkWinuiUrl; Dir = "microsoft.windowsappsdk.winui.1.8.260505002" },
    @{ Name = $assets.windowsAppSdkInteractive; Url = $assets.windowsAppSdkInteractiveUrl; Dir = "microsoft.windowsappsdk.interactiveexperiences.1.8.260430001" }
)
foreach ($package in $nugetPackages) {
    $archive = Get-AssetFile -Name $package.Name -Url $package.Url
    Expand-Package -Archive $archive -Destination (Join-Path $winuiDir $package.Dir)
}

$downloads = @(
    @{ Name = $assets.librime; Url = $assets.librimeUrl; Dest = Join-Path $librimeDir "rime" },
    @{ Name = $assets.deps; Url = $assets.depsUrl; Dest = Join-Path $librimeDir "deps" },
    @{ Name = $assets.wanxiang; Url = $assets.wanxiangUrl; Dest = $wanxiangDir; Archive = $true },
    @{ Name = $assets.wanxiangGram; Url = $assets.wanxiangGramUrl; Dest = Join-Path $wanxiangDir $assets.wanxiangGram; Archive = $false }
)

$fontDownloads = @(
    @{
        Name = $assets.miSans
        Url = $assets.miSansUrl
        Files = @(
            @{ Source = "MiSans/ttf/MiSans-Regular.ttf"; Dest = "MiSans-Regular.ttf" },
            @{ Source = "MiSans/ttf/MiSans-Medium.ttf"; Dest = "MiSans-Medium.ttf" },
            @{ Source = "MiSans/ttf/MiSans-Semibold.ttf"; Dest = "MiSans-Semibold.ttf" }
        )
    },
    @{
        Name = $assets.miSansTc
        Url = $assets.miSansTcUrl
        Files = @(
            @{ Source = "MiSans TC/ttf/MisansTC-Regular.ttf"; Dest = "MiSansTC-Regular.ttf" },
            @{ Source = "MiSans TC/ttf/MisansTC-Medium.ttf"; Dest = "MiSansTC-Medium.ttf" },
            @{ Source = "MiSans TC/ttf/MisansTC-Semibold.ttf"; Dest = "MiSansTC-Semibold.ttf" }
        )
    },
    @{
        Name = $assets.miSansL3
        Url = $assets.miSansL3Url
        Files = @(
            @{ Source = "MiSans L3/MiSans L3.ttf"; Dest = "MiSansL3-Regular.ttf" }
        )
    }
)

foreach ($item in $downloads) {
    $archive = Get-AssetFile -Name $item.Name -Url $item.Url

    if ($item.Archive -eq $false) {
        if ($Force -or -not (Test-Path -LiteralPath $item.Dest)) {
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $item.Dest) | Out-Null
            Copy-Item -LiteralPath $archive -Destination $item.Dest -Force
            Write-Host "Copied $archive -> $($item.Dest)"
        } else {
            Write-Host "Using copied $($item.Dest)"
        }
        continue
    }

    if ($Force -or -not (Test-Path -LiteralPath $item.Dest)) {
        if (Test-Path -LiteralPath $item.Dest) {
            Remove-Item -LiteralPath $item.Dest -Recurse -Force
        }
        Expand-Package -Archive $archive -Destination $item.Dest
    } else {
        Write-Host "Using extracted $($item.Dest)"
    }
}

foreach ($font in $fontDownloads) {
    $archive = Get-AssetFile -Name $font.Name -Url $font.Url

    $extractDir = Join-Path $downloadsDir ([IO.Path]::GetFileNameWithoutExtension($font.Name))
    $needsExtract = $Force -or -not (Test-Path -LiteralPath $extractDir)
    if (-not $needsExtract) {
        foreach ($file in $font.Files) {
            if (-not (Test-Path -LiteralPath (Join-Path $fontDir $file.Dest))) {
                $needsExtract = $true
                break
            }
        }
    }
    if ($needsExtract) {
        if (Test-Path -LiteralPath $extractDir) {
            Remove-Item -LiteralPath $extractDir -Recurse -Force
        }
        New-Item -ItemType Directory -Force -Path $extractDir | Out-Null
        Write-Host "Extracting $archive -> $extractDir"
        tar -xf $archive -C $extractDir
    } else {
        Write-Host "Using extracted $extractDir"
    }

    foreach ($file in $font.Files) {
        $source = Join-Path $extractDir $file.Source
        $dest = Join-Path $fontDir $file.Dest
        if (-not (Test-Path -LiteralPath $source)) {
            throw "Missing font inside archive: $source"
        }
        if ($Force -or -not (Test-Path -LiteralPath $dest)) {
            Copy-Item -LiteralPath $source -Destination $dest -Force
            Write-Host "Copied $source -> $dest"
        } else {
            Write-Host "Using copied $dest"
        }
    }
}

$extraFontDownloads = @(
    @{
        Name = $assets.sourceHanSansSc
        Url = $assets.sourceHanSansScUrl
        Files = @(@{ Source = "OTF/SimplifiedChinese/SourceHanSansSC-Regular.otf"; Dest = "SourceHanSansSC-Regular.otf" })
    },
    @{
        Name = $assets.sourceHanSansTc
        Url = $assets.sourceHanSansTcUrl
        Files = @(@{ Source = "OTF/TraditionalChinese/SourceHanSansTC-Regular.otf"; Dest = "SourceHanSansTC-Regular.otf" })
    }
)

foreach ($font in $extraFontDownloads) {
    $archive = Get-AssetFile -Name $font.Name -Url $font.Url
    $extractDir = Join-Path $downloadsDir ([IO.Path]::GetFileNameWithoutExtension($font.Name))
    if ($Force -or -not (Test-Path -LiteralPath $extractDir)) {
        if (Test-Path -LiteralPath $extractDir) {
            Remove-Item -LiteralPath $extractDir -Recurse -Force
        }
        New-Item -ItemType Directory -Force -Path $extractDir | Out-Null
        Write-Host "Extracting $archive -> $extractDir"
        tar -xf $archive -C $extractDir
    } else {
        Write-Host "Using extracted $extractDir"
    }
    foreach ($file in $font.Files) {
        $source = Join-Path $extractDir $file.Source
        $dest = Join-Path $fontDir $file.Dest
        if (-not (Test-Path -LiteralPath $source)) {
            throw "Missing font inside archive: $source"
        }
        if ($Force -or -not (Test-Path -LiteralPath $dest)) {
            Copy-Item -LiteralPath $source -Destination $dest -Force
            Write-Host "Copied $source -> $dest"
        } else {
            Write-Host "Using copied $dest"
        }
    }
}

$directFonts = @(
    @{ Name = $assets.plangothicP1; Url = $assets.plangothicP1Url },
    @{ Name = $assets.plangothicP2; Url = $assets.plangothicP2Url }
)
foreach ($font in $directFonts) {
    $source = Get-AssetFile -Name $font.Name -Url $font.Url
    $dest = Join-Path $fontDir $font.Name
    if ($Force -or -not (Test-Path -LiteralPath $dest)) {
        Copy-Item -LiteralPath $source -Destination $dest -Force
        Write-Host "Copied $source -> $dest"
    } else {
        Write-Host "Using copied $dest"
    }
}

Write-Host "Packages are ready."
