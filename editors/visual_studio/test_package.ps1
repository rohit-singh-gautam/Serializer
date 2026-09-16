#Requires -Version 5.1
<#
.SYNOPSIS
Check the built VSIX's registration and ensure its shared assets match the source.
.PARAMETER VsixPath
Optional package path. Defaults to the repository output for the manifest version.
#>
[CmdletBinding()]
param([string]$VsixPath)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
[xml]$sourceManifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'source.extension.vsixmanifest') -Raw
$identity = $sourceManifest.PackageManifest.Metadata.Identity
if (-not $VsixPath) {
  $VsixPath = Join-Path $repository "out/extensions/serializer-visual-studio-$($identity.Version).vsix"
}

# Read a required ZIP entry, failing explicitly when packaging omitted it.
function Read-PackageText {
  param([IO.Compression.ZipArchive]$Archive, [string]$Name)
  $entry = $Archive.GetEntry($Name)
  if ($null -eq $entry) { throw "Missing VSIX entry: $Name" }
  $reader = [IO.StreamReader]::new($entry.Open())
  try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
}

$archive = [IO.Compression.ZipFile]::OpenRead($VsixPath)
try {
  [xml]$manifest = Read-PackageText $archive 'extension.vsixmanifest'
  $packagedIdentity = $manifest.PackageManifest.Metadata.Identity
  if ($packagedIdentity.Id -cne $identity.Id -or $packagedIdentity.Version -cne $identity.Version) {
    throw 'The package identity/version differs from the source manifest.'
  }
  $target = $manifest.PackageManifest.Installation.InstallationTarget
  if ($target.Id -cne 'Microsoft.VisualStudio.Community' -or
      $target.Version -cne '[17.0,)' -or $target.ProductArchitecture -cne 'amd64') {
    throw 'Unexpected Visual Studio installation target.'
  }
  $asset = $manifest.PackageManifest.Assets.Asset
  if ($asset.Type -cne 'Microsoft.VisualStudio.VsPackage' -or $asset.Path -cne 'serializer.pkgdef') {
    throw 'Missing Visual Studio package registration.'
  }
  $sharedFiles = @{
    'Grammars/serializer.tmLanguage.json' = 'editors/serializer.tmLanguage.json'
    'language-configuration.json' = 'editors/vscode/language-configuration.json'
    'serializer.pkgdef' = 'editors/visual_studio/serializer.pkgdef'
    'LICENSE' = 'LICENSE'
    'Resources/serializer_logo_128x128.png' = 'logo/serializer_logo_128x128.png'
  }
  foreach ($name in $sharedFiles.Keys) {
    $entry = $archive.GetEntry($name)
    if ($null -eq $entry) { throw "Missing VSIX entry: $name" }
    $stream = $entry.Open()
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
      $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '')
    } finally {
      $sha.Dispose()
      $stream.Dispose()
    }
    $expected = (Get-FileHash -LiteralPath (Join-Path $repository $sharedFiles[$name]) -Algorithm SHA256).Hash
    if ($actual -cne $expected) { throw "Packaged asset differs from source: $name" }
  }
  foreach ($path in @($manifest.PackageManifest.Metadata.Icon, $manifest.PackageManifest.Metadata.License)) {
    if ($null -eq $archive.GetEntry($path.Replace('\', '/'))) {
      throw "Missing manifest asset: $path"
    }
  }
  $grammar = (Read-PackageText $archive 'Grammars/serializer.tmLanguage.json') | ConvertFrom-Json
  if ($grammar.scopeName -cne 'source.serializer' -or
      @($grammar.fileTypes).Count -ne 1 -or $grammar.fileTypes[0] -cne 'serializer') {
    throw 'The grammar must associate only .serializer files with source.serializer.'
  }
  $registration = Read-PackageText $archive 'serializer.pkgdef'
  foreach ($required in @(
      '[$RootKey$\TextMate\Repositories]',
      '"RohitSerializer"="$PackageFolder$\Grammars"',
      '[$RootKey$\TextMate\LanguageConfiguration\GrammarMapping]',
      '"source.serializer"="$PackageFolder$\language-configuration.json"')) {
    if (-not $registration.Contains($required)) { throw "Missing registration: $required" }
  }
  if (@($archive.Entries | Where-Object { $_.FullName -match '\.(dll|exe|pdb)$' }).Count -ne 0) {
    throw 'This grammar-only package must not contain compiled binaries.'
  }
} finally {
  $archive.Dispose()
}
Write-Host "Validated Visual Studio package: $VsixPath"
