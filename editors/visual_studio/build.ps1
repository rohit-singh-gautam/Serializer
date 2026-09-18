#Requires -Version 5.1
<#
.SYNOPSIS
Build and validate the Serializer Visual Studio VSIX without installing it.
.DESCRIPTION
Uses Visual Studio's MSBuild and locked NuGet dependencies. Run from any directory.
.PARAMETER MSBuildPath
Optional full path to Visual Studio's MSBuild.exe; otherwise discovered with vswhere.
#>
[CmdletBinding()]
param([string]$MSBuildPath)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot '../vscode/node_modules/esbuild/package.json'))) {
  throw 'Run npm ci in editors/vscode first. Building both packages uses the same locked TypeScript bundler.'
}

if (-not $MSBuildPath) {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
  if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw 'Install Visual Studio 2022/2026 or Build Tools with MSBuild, or pass -MSBuildPath.'
  }
  $candidates = @(& $vswhere -latest -products '*' -version '[17.0,)' `
    -requires Microsoft.Component.MSBuild -find 'MSBuild\Current\Bin\MSBuild.exe')
  if ($LASTEXITCODE -ne 0 -or $candidates.Count -eq 0) {
    throw 'No supported MSBuild was found. Install Visual Studio Build Tools or pass -MSBuildPath.'
  }
  $MSBuildPath = $candidates[0]
}

$msbuild = Get-Command -Name $MSBuildPath -CommandType Application -ErrorAction Stop
# Regenerate cached VSIX metadata so a version change cannot retain the previous identity.
& $msbuild.Source (Join-Path $PSScriptRoot 'serializer_language.csproj') /restore /t:Rebuild `
  /p:Configuration=Release /p:RestoreLockedMode=true /v:minimal /nologo
if ($LASTEXITCODE -ne 0) {
  throw "Visual Studio extension build failed with exit code $LASTEXITCODE."
}
& (Join-Path $PSScriptRoot 'test_package.ps1')
