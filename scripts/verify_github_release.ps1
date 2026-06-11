param(
  [string]$Repo = "founderyang/FluentPinyin",

  [string]$Tag = "v00.00.08",

  [string]$ExpectedAssetName = "FluentPinyin.msi",

  [string[]]$RequiredAssets = @(
    "FluentPinyin.msi",
    "payload-size-report.txt",
    "payload-resource-manifest.json"
  ),

  [string]$ExpectedPublisherSha256Thumbprint = "",

  [string]$ExpectedPublisherSha1Thumbprint = "",

  [string]$DownloadDir = "",

  [string]$Token = "",

  [string]$Proxy = "",

  [int]$Attempts = 12,

  [int]$DelaySeconds = 10,

  [switch]$AllowDraft,

  [switch]$AllowPrerelease,

  [switch]$SkipLatestCheck,

  [switch]$SkipSignatureCheck
)

$ErrorActionPreference = "Stop"

function Normalize-Thumbprint {
  param([string]$Value)
  if ([string]::IsNullOrWhiteSpace($Value)) {
    return ""
  }
  return ([regex]::Replace($Value, "[\s:-]", "")).ToUpperInvariant()
}

function Assert-Thumbprint {
  param(
    [string]$Name,
    [string]$Value,
    [int]$Length
  )
  $normalized = Normalize-Thumbprint $Value
  if ($normalized -notmatch "^[0-9A-F]{$Length}$") {
    throw "$Name must be a $Length-character hexadecimal certificate thumbprint."
  }
  return $normalized
}

function Get-CertificateSha256Thumbprint {
  param([System.Security.Cryptography.X509Certificates.X509Certificate2]$Certificate)
  $sha256 = [System.Security.Cryptography.SHA256]::Create()
  try {
    $digest = $sha256.ComputeHash($Certificate.RawData)
    return -join ($digest | ForEach-Object { $_.ToString("X2") })
  } finally {
    $sha256.Dispose()
  }
}

function Resolve-SignTool {
  $command = Get-Command signtool.exe -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($command) {
    return $command.Source
  }

  $programFilesX86 = ${env:ProgramFiles(x86)}
  if (-not [string]::IsNullOrWhiteSpace($programFilesX86)) {
    $kitsRoot = Join-Path $programFilesX86 "Windows Kits\10\bin"
    if (Test-Path -LiteralPath $kitsRoot) {
      $signtool = Get-ChildItem -LiteralPath $kitsRoot -Recurse -Filter signtool.exe |
        Where-Object { $_.FullName -match "\\x64\\signtool.exe$" } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
      if ($signtool) {
        return $signtool.FullName
      }
    }
  }

  throw "signtool.exe was not found. Install the Windows SDK or run this check on the release runner."
}

function New-GitHubHeaders {
  param(
    [string]$AccessToken,
    [string]$Accept
  )
  $headers = @{
    Accept = $Accept
    "X-GitHub-Api-Version" = "2022-11-28"
    "User-Agent" = "FluentPinyin-release-verify"
  }
  if (-not [string]::IsNullOrWhiteSpace($AccessToken)) {
    $headers.Authorization = "Bearer $AccessToken"
  }
  return $headers
}

function Add-OptionalProxy {
  param([hashtable]$Params)
  if (-not [string]::IsNullOrWhiteSpace($Proxy)) {
    $Params.Proxy = $Proxy
  }
  return $Params
}

function Get-ReleaseByTag {
  $headers = New-GitHubHeaders -AccessToken $Token -Accept "application/vnd.github+json"
  $uri = "https://api.github.com/repos/$Repo/releases/tags/$Tag"
  $params = Add-OptionalProxy @{
    Method = "Get"
    Uri = $uri
    Headers = $headers
  }
  try {
    return Invoke-RestMethod @params
  } catch {
    if (-not $AllowDraft) {
      throw
    }

    $listParams = Add-OptionalProxy @{
      Method = "Get"
      Uri = "https://api.github.com/repos/$Repo/releases?per_page=100"
      Headers = $headers
    }
    $releaseResponse = Invoke-RestMethod @listParams
    $releases = @($releaseResponse)
    if ($releases.Count -eq 1 -and $releases[0] -is [array]) {
      $releases = @($releases[0])
    }
    $match = $releases | Where-Object { $_.tag_name -eq $Tag } | Select-Object -First 1
    if ($match) {
      return $match
    }
    throw
  }
}

function Get-LatestRelease {
  $headers = New-GitHubHeaders -AccessToken $Token -Accept "application/vnd.github+json"
  $params = Add-OptionalProxy @{
    Method = "Get"
    Uri = "https://api.github.com/repos/$Repo/releases/latest"
    Headers = $headers
  }
  return Invoke-RestMethod @params
}

function Test-ReleaseState {
  param([object]$Release)
  $errors = New-Object System.Collections.Generic.List[string]

  if ($Release.tag_name -ne $Tag) {
    $errors.Add("release tag is '$($Release.tag_name)', expected '$Tag'")
  }
  if (-not $AllowDraft -and [bool]$Release.draft) {
    $errors.Add("release is still a draft")
  }
  if (-not $AllowPrerelease -and [bool]$Release.prerelease) {
    $errors.Add("release is marked as prerelease")
  }

  $assetNames = @($Release.assets | ForEach-Object { $_.name })
  foreach ($name in $RequiredAssets) {
    if ($assetNames -notcontains $name) {
      $errors.Add("missing release asset: $name")
    }
  }

  if (-not $AllowDraft -and -not $AllowPrerelease -and -not $SkipLatestCheck) {
    try {
      $latest = Get-LatestRelease
      if ($latest.tag_name -ne $Tag) {
        $errors.Add("latest release tag is '$($latest.tag_name)', expected '$Tag'")
      }
    } catch {
      $errors.Add("latest release is not ready: $($_.Exception.Message)")
    }
  }

  return $errors
}

if ([string]::IsNullOrWhiteSpace($Token)) {
  $Token = $env:GH_TOKEN
}
if ([string]::IsNullOrWhiteSpace($Token)) {
  $Token = $env:GITHUB_TOKEN
}
if ([string]::IsNullOrWhiteSpace($ExpectedPublisherSha256Thumbprint)) {
  $ExpectedPublisherSha256Thumbprint = $env:FP_UPDATER_PUBLISHER_THUMBPRINTS
}
if ([string]::IsNullOrWhiteSpace($ExpectedPublisherSha1Thumbprint)) {
  $ExpectedPublisherSha1Thumbprint = $env:WINDOWS_SIGNING_CERTIFICATE_SHA1
}

$expectedSha256 = ""
$expectedSha1 = ""
if (-not $SkipSignatureCheck) {
  $expectedSha256 = Assert-Thumbprint -Name "ExpectedPublisherSha256Thumbprint" `
    -Value $ExpectedPublisherSha256Thumbprint -Length 64
  if (-not [string]::IsNullOrWhiteSpace($ExpectedPublisherSha1Thumbprint)) {
    $expectedSha1 = Assert-Thumbprint -Name "ExpectedPublisherSha1Thumbprint" `
      -Value $ExpectedPublisherSha1Thumbprint -Length 40
  }
}

$release = $null
$lastErrors = @()
for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
  try {
    $release = Get-ReleaseByTag
    $lastErrors = @(Test-ReleaseState -Release $release)
    if ($lastErrors.Count -eq 0) {
      break
    }
  } catch {
    $lastErrors = @($_.Exception.Message)
  }

  if ($attempt -lt $Attempts) {
    Write-Host "Release is not ready yet ($attempt/$Attempts): $($lastErrors -join '; ')"
    Start-Sleep -Seconds $DelaySeconds
  }
}

if (-not $release) {
  throw "Release $Tag was not found for $Repo. Last error: $($lastErrors -join '; ')"
}
if ($lastErrors.Count -gt 0) {
  throw "Release $Tag failed verification: $($lastErrors -join '; ')"
}

$assets = @($release.assets)
$asset = $assets | Where-Object { $_.name -eq $ExpectedAssetName } | Select-Object -First 1
if (-not $asset) {
  throw "Release asset '$ExpectedAssetName' was not found."
}

if ([bool]$release.draft) {
  Write-Host "Draft release $Tag has the required assets."
} else {
  Write-Host "Release $Tag is published with required assets."
}

if ($SkipSignatureCheck) {
  Write-Host "Signature verification skipped."
  Write-Host "GitHub release verification passed."
  exit 0
}

if ([string]::IsNullOrWhiteSpace($DownloadDir)) {
  $DownloadDir = Join-Path ([IO.Path]::GetTempPath()) ("fluent-pinyin-release-" + [guid]::NewGuid().ToString("N"))
}
New-Item -ItemType Directory -Force -Path $DownloadDir | Out-Null
$downloadPath = Join-Path $DownloadDir $ExpectedAssetName
if (Test-Path -LiteralPath $downloadPath) {
  Remove-Item -LiteralPath $downloadPath -Force
}

$downloadHeaders = New-GitHubHeaders -AccessToken $Token -Accept "application/octet-stream"
$downloadParams = Add-OptionalProxy @{
  Method = "Get"
  Uri = $asset.url
  Headers = $downloadHeaders
  OutFile = $downloadPath
  MaximumRedirection = 5
}
Invoke-WebRequest @downloadParams

$downloaded = Get-Item -LiteralPath $downloadPath
if ($downloaded.Length -le 0) {
  throw "Downloaded MSI is empty: $downloadPath"
}
if ([int64]$asset.size -gt 0 -and $downloaded.Length -ne [int64]$asset.size) {
  throw "Downloaded MSI size $($downloaded.Length) does not match release asset size $($asset.size)."
}

$signtool = Resolve-SignTool
& $signtool verify /pa /v $downloadPath
if ($LASTEXITCODE -ne 0) {
  throw "signtool verification failed for $downloadPath"
}

$signature = Get-AuthenticodeSignature -LiteralPath $downloadPath
if ($signature.Status -ne "Valid") {
  throw "Authenticode signature is not valid: $($signature.StatusMessage)"
}
if (-not $signature.SignerCertificate) {
  throw "Authenticode signature does not expose a signer certificate."
}

$actualSha256 = Get-CertificateSha256Thumbprint -Certificate $signature.SignerCertificate
if ($actualSha256 -ne $expectedSha256) {
  throw "Signer SHA-256 thumbprint $actualSha256 does not match expected $expectedSha256."
}

if (-not [string]::IsNullOrWhiteSpace($expectedSha1)) {
  $actualSha1 = Normalize-Thumbprint $signature.SignerCertificate.Thumbprint
  if ($actualSha1 -ne $expectedSha1) {
    throw "Signer SHA-1 thumbprint $actualSha1 does not match expected $expectedSha1."
  }
}

Write-Host "MSI signature is valid and matches the expected publisher certificate."
Write-Host "GitHub release verification passed."
