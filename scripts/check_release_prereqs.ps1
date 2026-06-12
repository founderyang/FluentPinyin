param(
  [string]$Repo = "founderyang/FluentPinyin",

  [string]$Tag = "v00.00.08",

  [string[]]$RequiredDraftAssets = @(
    "RELEASE_NOTES_00.00.08.md",
    "payload-size-report.txt",
    "payload-resource-manifest.json"
  ),

  [string[]]$RequiredSecrets = @(
    "WINDOWS_SIGNING_CERTIFICATE_PFX_BASE64",
    "WINDOWS_SIGNING_CERTIFICATE_PASSWORD",
    "WINDOWS_SIGNING_CERTIFICATE_SHA256",
    "WINDOWS_SIGNING_CERTIFICATE_SHA1"
  ),

  [string]$Token = "",

  [string]$Proxy = ""
)

$ErrorActionPreference = "Stop"

function New-GitHubHeaders {
  param([string]$AccessToken)
  $headers = @{
    Accept = "application/vnd.github+json"
    "X-GitHub-Api-Version" = "2022-11-28"
    "User-Agent" = "FluentPinyin-release-prereq-check"
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

function Invoke-GitHub {
  param([string]$Uri)
  $params = Add-OptionalProxy @{
    Method = "Get"
    Uri = $Uri
    Headers = $headers
  }
  return Invoke-RestMethod @params
}

function Add-Check {
  param(
    [System.Collections.Generic.List[object]]$Checks,
    [string]$Name,
    [bool]$Ok,
    [string]$Detail
  )
  $Checks.Add([pscustomobject]@{
      name = $Name
      ok = $Ok
      detail = $Detail
    }) | Out-Null
}

if ([string]::IsNullOrWhiteSpace($Token)) {
  $Token = $env:GH_TOKEN
}
if ([string]::IsNullOrWhiteSpace($Token)) {
  $Token = $env:GITHUB_TOKEN
}
if ([string]::IsNullOrWhiteSpace($Token)) {
  throw "A GitHub token is required. Set GH_TOKEN/GITHUB_TOKEN or pass -Token."
}

$headers = New-GitHubHeaders -AccessToken $Token
$checks = New-Object System.Collections.Generic.List[object]

$workflows = Invoke-GitHub "https://api.github.com/repos/$Repo/actions/workflows"
$releaseWorkflow = $workflows.workflows |
  Where-Object { $_.path -eq ".github/workflows/release.yml" } |
  Select-Object -First 1
Add-Check $checks "release-workflow-active" `
  ($null -ne $releaseWorkflow -and $releaseWorkflow.state -eq "active") `
  $(if ($releaseWorkflow) { "$($releaseWorkflow.state) id=$($releaseWorkflow.id)" } else { "missing" })

$mainRef = Invoke-GitHub "https://api.github.com/repos/$Repo/git/ref/heads/main"
$tagRefName = $Tag -replace "^refs/tags/", ""
$tagRef = Invoke-GitHub "https://api.github.com/repos/$Repo/git/ref/tags/$tagRefName"
$mainSha = $mainRef.object.sha
$tagSha = $tagRef.object.sha
Add-Check $checks "tag-matches-main" ($mainSha -eq $tagSha) "main=$mainSha tag=$tagSha"

$releases = @(Invoke-GitHub "https://api.github.com/repos/$Repo/releases?per_page=100")
if ($releases.Count -eq 1 -and $releases[0] -is [array]) {
  $releases = @($releases[0])
}
$draftRelease = $releases |
  Where-Object { $_.tag_name -eq $Tag } |
  Select-Object -First 1
Add-Check $checks "draft-release-exists" ($null -ne $draftRelease) `
  $(if ($draftRelease) { "id=$($draftRelease.id) draft=$($draftRelease.draft)" } else { "missing" })
if ($draftRelease) {
  Add-Check $checks "release-is-draft" ([bool]$draftRelease.draft) "draft=$($draftRelease.draft)"
  $assetNames = @($draftRelease.assets | ForEach-Object { $_.name })
  foreach ($assetName in $RequiredDraftAssets) {
    Add-Check $checks "draft-asset-$assetName" ($assetNames -contains $assetName) `
      $(if ($assetNames -contains $assetName) { "present" } else { "missing" })
  }
}

$secretResponse = Invoke-GitHub "https://api.github.com/repos/$Repo/actions/secrets"
$secretNames = @($secretResponse.secrets | ForEach-Object { $_.name })
foreach ($secretName in $RequiredSecrets) {
  Add-Check $checks "secret-$secretName" ($secretNames -contains $secretName) `
    $(if ($secretNames -contains $secretName) { "present" } else { "missing" })
}

$failed = @($checks | Where-Object { -not $_.ok })
foreach ($check in $checks) {
  $status = if ($check.ok) { "ok" } else { "missing" }
  Write-Host ("{0}: {1}: {2}" -f $check.name, $status, $check.detail)
}

if ($failed.Count -gt 0) {
  Write-Host ""
  Write-Host "Release prerequisites are not complete."
  exit 1
}

Write-Host ""
Write-Host "Release prerequisites are complete. It is safe to trigger the signed Release workflow."
