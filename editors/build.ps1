#Requires -Version 5.1
<#
.SYNOPSIS
Build both Rohit Serializer extension packages without installing them.
.DESCRIPTION
Restore locked npm dependencies, package VS Code, then build and validate the
Visual Studio VSIX. Requires Windows, Node.js 22+, npm and Visual Studio MSBuild.
Run from any directory. Both manifests must specify the same release version.
.PARAMETER MSBuildPath
Optional full path to Visual Studio's MSBuild.exe; otherwise discovered with vswhere.
#>
[CmdletBinding()]
param([string]$MSBuildPath)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Resolve native tools rather than execution-policy-sensitive PowerShell shims.
function Resolve-BuildApplication {
  param([string]$Name)
  $application = Get-Command -Name $Name -CommandType Application -ErrorAction SilentlyContinue |
    Select-Object -First 1
  if ($null -eq $application) { throw "Cannot find '$Name'. Install Node.js 22+ and npm and add them to PATH." }
  return $application.Source
}

# Stop immediately on tool failure so a stale package cannot represent a successful build.
function Invoke-ExtensionCommand {
  param([string]$Command, [string[]]$Arguments)
  & $Command @Arguments
  if ($LASTEXITCODE -ne 0) { throw "Extension build command '$Command' failed with exit code $LASTEXITCODE." }
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
  throw 'Building both editor extensions requires Windows and Visual Studio MSBuild.'
}
$vscodeDirectory = Join-Path $PSScriptRoot 'vscode'
$manifest = Get-Content -LiteralPath (Join-Path $vscodeDirectory 'package.json') -Raw | ConvertFrom-Json
[xml]$vsManifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'visual_studio/source.extension.vsixmanifest') -Raw
$version = [string]$manifest.version
if ($version -notmatch '^\d+\.\d+\.\d+$' -or $version -cne [string]$vsManifest.PackageManifest.Metadata.Identity.Version) {
  throw 'The Visual Studio and Visual Studio Code extension manifests must have the same major.minor.patch version.'
}
$node = Resolve-BuildApplication 'node'
$npm = Resolve-BuildApplication 'npm'
$nodeVersion = Invoke-ExtensionCommand $node @('--version')
if ($nodeVersion -notmatch '^v(\d+)\.' -or [int]$Matches[1] -lt 22) {
  throw "Node.js 22 or newer is required to build the extensions; found '$nodeVersion'."
}

$outputDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../out/extensions'))
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$vscodePackage = Join-Path $outputDirectory "serializer-vscode-$version.vsix"
Write-Host "Building both Rohit Serializer extensions at version $version..."
Push-Location -LiteralPath $vscodeDirectory
try {
  Invoke-ExtensionCommand $npm @('ci', '--no-audit', '--no-fund')
  # vsce invokes vscode:prepublish to compile and refresh every canonical asset.
  Invoke-ExtensionCommand $npm @('exec', '--offline', '--', 'vsce', 'package', '--no-dependencies', '--out', $vscodePackage)
} finally {
  Pop-Location
}
& (Join-Path $PSScriptRoot 'visual_studio/build.ps1') -MSBuildPath $MSBuildPath
Write-Host "Built both extension packages in $outputDirectory."
