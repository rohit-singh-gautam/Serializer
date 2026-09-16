#Requires -Version 5.1
<#
.SYNOPSIS
Build and install the local Serializer VS Code extension.
.DESCRIPTION
Restore locked npm dependencies, package the extension, and install its VSIX.
Paths are relative to this script, so it can be invoked from any directory.
Requires VS Code's CLI and, unless -SkipBuild is used, Node.js 22+ and npm.
.PARAMETER CodeCommand
VS Code CLI command or full path, such as code, code-insiders, or a code.cmd path.
.PARAMETER SkipBuild
Install the existing VSIX for the manifest version without restoring or building.
.PARAMETER ExtensionsDirectory
Optional separate extensions directory, useful for an isolated installation.
#>
[CmdletBinding()]
param(
  [string]$CodeCommand = 'code',
  [switch]$SkipBuild,
  [string]$ExtensionsDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Resolve a native CLI rather than a PowerShell shim that may be execution-policy blocked.
function Resolve-Application {
  param([Parameter(Mandatory = $true)][string]$Name)
  $application = Get-Command -Name $Name -CommandType Application -ErrorAction SilentlyContinue |
    Select-Object -First 1
  if ($null -eq $application) {
    throw "Cannot find '$Name'. Install it and add its CLI to PATH, or pass -CodeCommand for VS Code."
  }
  return $application.Source
}

# Stop immediately on native-tool failure instead of installing a stale package.
function Invoke-CheckedCommand {
  param(
    [Parameter(Mandatory = $true)][string]$FilePath,
    [Parameter(Mandatory = $true)][string[]]$CommandArguments
  )
  & $FilePath @CommandArguments
  if ($LASTEXITCODE -ne 0) {
    throw "Command '$FilePath' failed with exit code $LASTEXITCODE."
  }
}

$codePath = Resolve-Application -Name $CodeCommand
$extensionDirectory = Join-Path $PSScriptRoot 'editors/vscode'
$manifest = Get-Content -LiteralPath (Join-Path $extensionDirectory 'package.json') -Raw |
  ConvertFrom-Json
$version = [string]$manifest.version
if ($version -notmatch '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$') {
  throw "Invalid extension version in editors/vscode/package.json: '$version'."
}
$vsixPath = Join-Path $PSScriptRoot "out/extensions/serializer-vscode-$version.vsix"
$codeArguments = @('--install-extension', $vsixPath, '--force')
if ($ExtensionsDirectory) {
  # Resolve before changing directory so relative paths retain the caller's meaning.
  $absoluteExtensionsDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath(
    $ExtensionsDirectory)
  $codeArguments += @('--extensions-dir', $absoluteExtensionsDirectory)
}

if (-not $SkipBuild) {
  $nodePath = Resolve-Application -Name 'node'
  $npmPath = Resolve-Application -Name 'npm'
  $nodeVersion = & $nodePath --version
  if ($LASTEXITCODE -ne 0 -or $nodeVersion -notmatch '^v(\d+)\.') {
    throw 'Unable to determine the installed Node.js version.'
  }
  if ([int]$Matches[1] -lt 22) {
    throw "Node.js 22 or newer is required; found $nodeVersion."
  }

  Push-Location -LiteralPath $extensionDirectory
  try {
    Write-Host 'Restoring extension dependencies...'
    Invoke-CheckedCommand -FilePath $npmPath -CommandArguments @('ci', '--no-audit', '--no-fund')
    Write-Host "Packaging Serializer $version..."
    # The local vsce runs vscode:prepublish, which builds and copies the canonical assets.
    # Select the filename from the manifest instead of repeating a release version here.
    Invoke-CheckedCommand -FilePath $npmPath -CommandArguments @(
      'exec', '--offline', '--', 'vsce', 'package', '--no-dependencies', '--out', $vsixPath)
  } finally {
    Pop-Location
  }
}

if (-not (Test-Path -LiteralPath $vsixPath -PathType Leaf)) {
  throw "VSIX not found: $vsixPath. Run this script without -SkipBuild to create it."
}
Write-Host "Installing Serializer $version..."
Invoke-CheckedCommand -FilePath $codePath -CommandArguments $codeArguments
Write-Host 'Serializer extension installed. Reload VS Code if prompted.'
