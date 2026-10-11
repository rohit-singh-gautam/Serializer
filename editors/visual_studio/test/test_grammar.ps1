#Requires -Version 5.1
<#
.SYNOPSIS
Check embedded syntax with real installed Visual Studio TextMate grammar assets.
.DESCRIPTION
Uses the repository's locked TextMate tokenizer. This is an asset integration test,
not an editor-host UI test. It also verifies fallback when native grammars are absent.
.PARAMETER InstallationPath
Optional Visual Studio installation directory; otherwise discovered with vswhere.
#>
[CmdletBinding()]
param([string]$InstallationPath)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $InstallationPath) {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
  if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw 'Install Visual Studio or pass -InstallationPath to inspect its TextMate grammar assets.'
  }
  $candidates = @(& $vswhere -latest -products '*' -version '[17.0,)' -property installationPath)
  if ($LASTEXITCODE -ne 0 -or $candidates.Count -eq 0) {
    throw 'No supported Visual Studio installation was found.'
  }
  $InstallationPath = $candidates[0]
}

$grammars = Join-Path $InstallationPath 'Common7/IDE/CommonExtensions/Microsoft/TextMate/Starterkit/Extensions'
if (-not (Test-Path -LiteralPath $grammars -PathType Container)) {
  throw "Visual Studio TextMate grammar assets are missing: $grammars"
}
& node (Join-Path $PSScriptRoot 'test_grammar.cjs') $grammars
if ($LASTEXITCODE -ne 0) { throw "Visual Studio grammar verification failed with exit code $LASTEXITCODE." }
