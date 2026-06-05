param(
    [string]$BuildDir = ".\build-release",
    [string]$Config = "Release",
    [string]$PayloadDir = ".\dist\FluentPinyin",
    [string]$ReleaseDir = ".\dist\release",
    [string]$ProductVersion = "",
    [switch]$SkipInstallers,
    [switch]$IncludeZip
)

$ErrorActionPreference = "Stop"

function Resolve-Tool([string]$Name, [string[]]$Fallbacks) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    foreach ($path in $Fallbacks) {
        if (Test-Path -LiteralPath $path) {
            return $path
        }
    }

    throw "Cannot find $Name. Install it first, then run this script again."
}

function ConvertTo-NsisPath([string]$Path) {
    return $Path.TrimEnd('\', '/') -replace '/', '\'
}

function ConvertTo-WixId([string]$Prefix, [string]$Value) {
    $clean = ($Value -replace "[^A-Za-z0-9_]", "_")
    if ($clean.Length -gt 56) {
        $clean = $clean.Substring(0, 56)
    }
    return "$Prefix`_$clean"
}

function Escape-Xml([string]$Value) {
    return [Security.SecurityElement]::Escape($Value)
}

function Add-WixDirectoryXml(
    [System.Text.StringBuilder]$Builder,
    [string]$DirectoryPath,
    [string]$DirectoryId,
    [string]$RelativePath,
    [System.Collections.Generic.List[string]]$ComponentIds,
    [int]$Indent
) {
    $indentText = " " * $Indent
    $files = Get-ChildItem -LiteralPath $DirectoryPath -File | Sort-Object Name
    foreach ($file in $files) {
        $relativeFile = if ([string]::IsNullOrEmpty($RelativePath)) {
            $file.Name
        } else {
            "$RelativePath\$($file.Name)"
        }
        $componentId = ConvertTo-WixId "cmp" $relativeFile
        $fileId = ConvertTo-WixId "fil" $relativeFile
        $ComponentIds.Add($componentId)
        [void]$Builder.AppendLine("$indentText<Component Id=""$componentId"" Guid=""*"">")
        [void]$Builder.AppendLine("$indentText  <File Id=""$fileId"" Source=""$(Escape-Xml $file.FullName)"" KeyPath=""yes"" />")
        [void]$Builder.AppendLine("$indentText</Component>")
    }

    $directories = Get-ChildItem -LiteralPath $DirectoryPath -Directory | Sort-Object Name
    foreach ($directory in $directories) {
        $relativeDirectory = if ([string]::IsNullOrEmpty($RelativePath)) {
            $directory.Name
        } else {
            "$RelativePath\$($directory.Name)"
        }
        $childDirectoryId = ConvertTo-WixId "dir" $relativeDirectory
        [void]$Builder.AppendLine("$indentText<Directory Id=""$childDirectoryId"" Name=""$(Escape-Xml $directory.Name)"">")
        Add-WixDirectoryXml `
            -Builder $Builder `
            -DirectoryPath $directory.FullName `
            -DirectoryId $childDirectoryId `
            -RelativePath $relativeDirectory `
            -ComponentIds $ComponentIds `
            -Indent ($Indent + 2)
        [void]$Builder.AppendLine("$indentText</Directory>")
    }
}

function New-WixPayloadFile([string]$PayloadPath, [string]$OutputPath) {
    $builder = [System.Text.StringBuilder]::new()
    $componentIds = [System.Collections.Generic.List[string]]::new()

    [void]$builder.AppendLine('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
    [void]$builder.AppendLine('  <Fragment>')
    [void]$builder.AppendLine('    <DirectoryRef Id="INSTALLFOLDER">')
    Add-WixDirectoryXml `
        -Builder $builder `
        -DirectoryPath $PayloadPath `
        -DirectoryId "INSTALLFOLDER" `
        -RelativePath "" `
        -ComponentIds $componentIds `
        -Indent 6
    [void]$builder.AppendLine('    </DirectoryRef>')
    [void]$builder.AppendLine('  </Fragment>')
    [void]$builder.AppendLine('  <Fragment>')
    [void]$builder.AppendLine('    <ComponentGroup Id="PayloadComponents">')
    foreach ($componentId in $componentIds) {
        [void]$builder.AppendLine("      <ComponentRef Id=""$componentId"" />")
    }
    [void]$builder.AppendLine('    </ComponentGroup>')
    [void]$builder.AppendLine('  </Fragment>')
    [void]$builder.AppendLine('</Wix>')

    [IO.File]::WriteAllText($OutputPath, $builder.ToString(), [Text.UTF8Encoding]::new($false))
}

function Copy-RequiredFile([string]$SourceDir, [string]$DestinationDir, [string]$FileName) {
    $source = Join-Path $SourceDir $FileName
    if (-not (Test-Path -LiteralPath $source)) {
        throw "Missing build artifact: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $DestinationDir $FileName) -Force
}

function Copy-OptionalFile([string]$SourceDir, [string]$DestinationDir, [string]$FileName) {
    $source = Join-Path $SourceDir $FileName
    if (Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $DestinationDir $FileName) -Force
    }
}

function Copy-DirectoryClean([string]$Source, [string]$Destination) {
    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Missing directory: $Source"
    }
    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
}

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    if ([string]::IsNullOrWhiteSpace($ProductVersion)) {
        $cmake = Get-Content -LiteralPath ".\CMakeLists.txt" -Raw
        if ($cmake -match "project\(FluentPinyin VERSION ([0-9.]+)") {
            $ProductVersion = $Matches[1]
        } else {
            throw "Cannot determine product version from CMakeLists.txt."
        }
    }

    $resolvedBuildDir = (Resolve-Path -Path $BuildDir -ErrorAction Stop).ProviderPath
    $binDir = Join-Path $resolvedBuildDir "bin\$Config"
    if (-not (Test-Path -LiteralPath $binDir)) {
        throw "Cannot find build output: $binDir"
    }

    $resolvedPayload = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($PayloadDir)
    $resolvedRelease = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ReleaseDir)
    if (Test-Path -LiteralPath $resolvedPayload) {
        Remove-Item -LiteralPath $resolvedPayload -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $resolvedPayload, $resolvedRelease | Out-Null

    $requiredFiles = @(
        "fluent-pinyin-tsf.dll",
        "fluent-pinyin-core.dll",
        "fluent-pinyin-ui.exe",
        "fluent-pinyin-settings.exe",
        "fluent-pinyin-updater.exe",
        "fluent-pinyin-devtools.exe",
        "rime.dll"
    )
    foreach ($file in $requiredFiles) {
        Copy-RequiredFile -SourceDir $binDir -DestinationDir $resolvedPayload -FileName $file
    }

    $optionalFiles = @(
        "fluent-pinyin.ico",
        "fluent-pinyin-dark.ico",
        "fluent-pinyin-light.ico",
        "Microsoft.WindowsAppRuntime.Bootstrap.dll",
        "resources.pri"
    )
    foreach ($file in $optionalFiles) {
        Copy-OptionalFile -SourceDir $binDir -DestinationDir $resolvedPayload -FileName $file
    }

    Copy-DirectoryClean -Source (Join-Path $binDir "rime-data") -Destination (Join-Path $resolvedPayload "rime-data")

    $sourceFonts = Join-Path $binDir "fonts"
    if (-not (Test-Path -LiteralPath $sourceFonts)) {
        $sourceFonts = Join-Path $root "assets\fonts"
    }
    Copy-DirectoryClean -Source $sourceFonts -Destination (Join-Path $resolvedPayload "fonts")

    $winuiRuntime = Join-Path $binDir "Microsoft.UI.Xaml"
    if (Test-Path -LiteralPath $winuiRuntime) {
        Copy-DirectoryClean -Source $winuiRuntime -Destination (Join-Path $resolvedPayload "Microsoft.UI.Xaml")
    }

    $scriptsDir = Join-Path $resolvedPayload "scripts"
    New-Item -ItemType Directory -Force -Path $scriptsDir | Out-Null
    Copy-Item -LiteralPath ".\scripts\install-fonts.ps1" -Destination (Join-Path $scriptsDir "install-fonts.ps1") -Force

    $readme = @"
FluentPinyin $ProductVersion

Install path: C:\Program Files\FluentPinyin

Use FluentPinyin-Setup.exe for installation.
Use FluentPinyin-Uninstall.exe to remove the app.
The MSI package is provided for managed installation.
"@
    [IO.File]::WriteAllText((Join-Path $resolvedPayload "README.txt"), $readme, [Text.UTF8Encoding]::new($false))

    if ($IncludeZip) {
        $archive = Join-Path $resolvedRelease "FluentPinyin-payload.zip"
        Compress-Archive -Path (Join-Path $resolvedPayload "*") -DestinationPath $archive -Force
        Write-Host "Archive: $archive"
    }

    if (-not $SkipInstallers) {
        $makensis = Resolve-Tool `
            -Name "makensis.exe" `
            -Fallbacks @("${env:ProgramFiles(x86)}\NSIS\makensis.exe")
        $wix = Resolve-Tool `
            -Name "wix.exe" `
            -Fallbacks @("${env:ProgramFiles}\WiX Toolset v7.0\bin\wix.exe")

        $payloadForNsis = ConvertTo-NsisPath $resolvedPayload
        $releaseForNsis = ConvertTo-NsisPath $resolvedRelease
        & $makensis `
            "/DPRODUCT_VERSION=$ProductVersion" `
            "/DPAYLOAD_DIR=$payloadForNsis" `
            "/DOUTPUT_DIR=$releaseForNsis" `
            ".\installer\fluent-pinyin.nsi"
        if ($LASTEXITCODE -ne 0) {
            throw "makensis failed with exit code $LASTEXITCODE"
        }

        & $makensis `
            "/DOUTPUT_DIR=$releaseForNsis" `
            ".\installer\fluent-pinyin-uninstall.nsi"
        if ($LASTEXITCODE -ne 0) {
            throw "makensis uninstall failed with exit code $LASTEXITCODE"
        }

        $payloadWxs = Join-Path $resolvedRelease "payload.wxs"
        New-WixPayloadFile -PayloadPath $resolvedPayload -OutputPath $payloadWxs
        & $wix `
            "build" `
            ".\installer\fluent-pinyin.wxs" `
            $payloadWxs `
            "-d" `
            "ProductVersion=$ProductVersion" `
            "-arch" `
            "x64" `
            "-o" `
            (Join-Path $resolvedRelease "FluentPinyin.msi")
        if ($LASTEXITCODE -ne 0) {
            throw "wix build failed with exit code $LASTEXITCODE"
        }
        Remove-Item -LiteralPath $payloadWxs -Force
        Remove-Item -LiteralPath (Join-Path $resolvedRelease "FluentPinyin.wixpdb") -Force -ErrorAction SilentlyContinue
    }

    Write-Host "Payload: $resolvedPayload"
    Write-Host "Release: $resolvedRelease"
} finally {
    Pop-Location
}
