param(
  [string]$Repo = "founderyang/FluentPinyin",

  [string]$Tag = "v00.00.08",

  [string]$Ref = "main",

  [string]$Token = "",

  [string]$Proxy = "",

  [int]$PollSeconds = 30,

  [int]$TimeoutMinutes = 90,

  [switch]$SkipPostPublishVerification
)

$ErrorActionPreference = "Stop"

function New-GitHubHeaders {
  param([string]$AccessToken)
  return @{
    Authorization = "Bearer $AccessToken"
    Accept = "application/vnd.github+json"
    "X-GitHub-Api-Version" = "2022-11-28"
    "User-Agent" = "FluentPinyin-signed-release-publish"
  }
}

function Add-OptionalProxy {
  param([hashtable]$Params)
  if (-not [string]::IsNullOrWhiteSpace($Proxy)) {
    $Params.Proxy = $Proxy
  }
  return $Params
}

function Invoke-GitHubJson {
  param(
    [string]$Method,
    [string]$Uri,
    [object]$Body = $null
  )
  $params = Add-OptionalProxy @{
    Method = $Method
    Uri = $Uri
    Headers = $headers
  }
  if ($null -ne $Body) {
    $params.Body = ($Body | ConvertTo-Json -Depth 10)
    $params.ContentType = "application/json"
  }
  return Invoke-RestMethod @params
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

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$prereqScript = Join-Path $scriptRoot "check_release_prereqs.ps1"
$verifyScript = Join-Path $scriptRoot "verify_github_release.ps1"

& $prereqScript -Repo $Repo -Tag $Tag -Token $Token -Proxy $Proxy
if ($LASTEXITCODE -ne 0) {
  throw "Release prerequisites are incomplete; refusing to trigger signed release."
}

$headers = New-GitHubHeaders -AccessToken $Token
$workflow = Invoke-GitHubJson -Method Get -Uri "https://api.github.com/repos/$Repo/actions/workflows/release.yml"
if ($workflow.state -ne "active") {
  throw "Release workflow is not active: $($workflow.state)"
}

$before = Invoke-GitHubJson -Method Get -Uri "https://api.github.com/repos/$Repo/actions/workflows/release.yml/runs?per_page=10"
$beforeIds = @($before.workflow_runs | ForEach-Object { [string]$_.id })

$dispatchBody = @{
  ref = $Ref
  inputs = @{
    tag = $Tag
    publish = "true"
  }
}
Invoke-GitHubJson -Method Post -Uri "https://api.github.com/repos/$Repo/actions/workflows/release.yml/dispatches" -Body $dispatchBody | Out-Null
Write-Host "Triggered Release workflow for $Tag on $Ref."

$deadline = (Get-Date).AddMinutes($TimeoutMinutes)
$run = $null
while ((Get-Date) -lt $deadline) {
  Start-Sleep -Seconds $PollSeconds
  $runs = Invoke-GitHubJson -Method Get -Uri "https://api.github.com/repos/$Repo/actions/workflows/release.yml/runs?per_page=10"
  $run = $runs.workflow_runs |
    Where-Object {
      ($beforeIds -notcontains [string]$_.id) -and
      $_.event -eq "workflow_dispatch" -and
      $_.head_branch -eq $Ref
    } |
    Sort-Object created_at -Descending |
    Select-Object -First 1
  if ($run) {
    break
  }
  Write-Host "Waiting for Release workflow run to appear..."
}
if (-not $run) {
  throw "Timed out waiting for the Release workflow run to appear."
}

Write-Host "Release run: $($run.html_url)"
while ((Get-Date) -lt $deadline) {
  $run = Invoke-GitHubJson -Method Get -Uri "https://api.github.com/repos/$Repo/actions/runs/$($run.id)"
  Write-Host "Release workflow status: $($run.status) $($run.conclusion)"
  if ($run.status -eq "completed") {
    if ($run.conclusion -ne "success") {
      throw "Release workflow failed: $($run.conclusion) $($run.html_url)"
    }
    break
  }
  Start-Sleep -Seconds $PollSeconds
}
if ($run.status -ne "completed") {
  throw "Timed out waiting for Release workflow to complete: $($run.html_url)"
}

if (-not $SkipPostPublishVerification) {
  & $verifyScript -Repo $Repo -Tag $Tag -Token $Token -Proxy $Proxy
  if ($LASTEXITCODE -ne 0) {
    throw "Published release verification failed."
  }
}

Write-Host "Signed release published and verified: $Tag"
