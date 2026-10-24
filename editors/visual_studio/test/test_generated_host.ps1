#Requires -Version 5.1
<#
.SYNOPSIS
Qualify the current installed package in a hidden real Visual Studio host.
.DESCRIPTION
Requires the matching locally built package to be installed. Generates all eleven
owning outputs in a disposable workspace, checks native reverse navigation and
live source offsets, and terminates only the automation host created by this test.
#>
param(
  [Parameter(Mandatory = $true)][string]$Compiler,
  [string]$ProgId = 'VisualStudio.DTE.18.0'
)
$ErrorActionPreference = 'Stop'
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
[xml]$manifest = Get-Content -LiteralPath (Join-Path $repository 'editors/visual_studio/source.extension.vsixmanifest') -Raw
$identity = $manifest.PackageManifest.Metadata.Identity
if ($ProgId -notmatch '^VisualStudio\.DTE\.(17|18)\.0$') {
  throw 'Select an installed Visual Studio 2022 or 2026 DTE ProgId.'
}
$profilePattern = "^$($Matches[1])\.0_[0-9a-f]+$"
$assembly = Join-Path $repository 'editors/visual_studio/bin/Release/Rohit.Serializer.VisualStudio.dll'
$expectedHash = (Get-FileHash -LiteralPath $assembly -Algorithm SHA256).Hash
$installed = @(Get-ChildItem (Join-Path $env:LOCALAPPDATA 'Microsoft/VisualStudio') -Directory |
  Where-Object Name -match $profilePattern | ForEach-Object {
    Get-ChildItem (Join-Path $_.FullName 'Extensions') -Filter extension.vsixmanifest -Recurse -ErrorAction SilentlyContinue
  } | Where-Object {
    [xml]$candidate = Get-Content -LiteralPath $_.FullName -Raw
    $candidate.PackageManifest.Metadata.Identity.Id -eq $identity.Id -and
      $candidate.PackageManifest.Metadata.Identity.Version -eq $identity.Version
  })
if ($installed.Count -ne 1) { throw 'Install exactly one current local Visual Studio package before host qualification.' }
$installedAssembly = Join-Path $installed[0].DirectoryName 'Rohit.Serializer.VisualStudio.dll'
if ((Get-FileHash -LiteralPath $installedAssembly -Algorithm SHA256).Hash -ne $expectedHash) {
  throw 'Installed package assembly does not match the current local build.'
}
Write-Output "Verified installed package $($identity.Version), assembly SHA256 $expectedHash"
$root = Join-Path $repository "out/extension-tests/vs-generated-host-$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path (Join-Path $root 'types') -Force | Out-Null
$schema = Join-Path $root 'model.serializer'
$included = Join-Path $root 'types/item.serializer'
[IO.File]::WriteAllText($schema, "serializer version 1;`ninclude types/middle;`nnamespace host_test { class envelope { public host_test::item owner (1); } }`n")
[IO.File]::WriteAllText((Join-Path $root 'types/middle.serializer'), "serializer version 1;`ninclude item;`n")
[IO.File]::WriteAllText($included, "serializer version 1;`nnamespace host_test { class item { public int32 value (1); } }`n")
$outputs = [ordered]@{ cpp='model.hpp'; java='Model.java'; js='model.mjs'; typescript='model.d.mts';
  go='model.go'; csharp='Model.cs'; rust='model.rs'; python='model.py'; swift='Model.swift'; kotlin='Model.kt'; c='model.h' }
$generated = @()
foreach ($language in $outputs.Keys) {
  $output = Join-Path $root "build/$language/$($outputs[$language])"
  New-Item -ItemType Directory -Path (Split-Path $output) -Force | Out-Null
  & $Compiler --input $schema --language $language "--$language.output" $output --include-mode inline --cpp.format false --depfile "$output.d"
  if ($LASTEXITCODE -ne 0) { throw "Actual $language generation failed." }
  $generated += $output
}

# Retry only the IDE's documented busy-call responses while native commands dispatch.
function Invoke-Ide {
  param([scriptblock]$Action)
  for ($attempt = 0; $attempt -lt 100; ++$attempt) {
    try { return ,(& $Action) } catch {
      if ($_.Exception.ToString() -notmatch 'rejected by callee|retry later|0x80010001|0x8001010A') { throw }
      Start-Sleep -Milliseconds 200
    }
  }
  throw 'Visual Studio remained busy.'
}

# Select exact generated declaration spelling with either native selection direction.
function Select-Token {
  param([string]$File, [string]$Token, [bool]$Reversed, [int]$Offset)
  Invoke-Ide { $dte.ItemOperations.OpenFile($File, '{7651a700-06e5-11d1-8ebd-00a0c90f26ea}') } | Out-Null
  for ($attempt = 0; $attempt -lt 50; ++$attempt) {
    $active = Invoke-Ide { $dte.ActiveDocument }
    if ($null -ne $active -and $active.FullName -eq $File -and $null -ne $active.Selection) { break }
    Start-Sleep -Milliseconds 200
  }
  if ($null -eq $active -or $active.FullName -ne $File -or $null -eq $active.Selection) {
    throw "Source document did not become active: $File"
  }
  $text = [IO.File]::ReadAllText($File)
  if ($Offset -lt 0 -or $text.Substring($Offset, $Token.Length) -cne $Token) {
    throw "Missing declaration $Token in $File"
  }
  $prefix = $text.Substring(0, $Offset)
  $line = ($prefix -split "`n").Count
  $column = $Offset - $prefix.LastIndexOf("`n")
  Invoke-Ide {
    $active.Activate()
    $selection = $active.Selection
    $selection.MoveToLineAndOffset($line, $column + $(if ($Reversed) { $Token.Length } else { 0 }), $false)
    $selection.MoveToLineAndOffset($line, $column + $(if ($Reversed) { 0 } else { $Token.Length }), $true)
  }
  Write-Output "SELECT $File ($Token), reversed=$Reversed"
}

# Require the actual native command's exact source word and live line after an unsaved edit.
function Assert-Declaration {
  param([string]$Expected, [string]$Token, [int]$Line)
  # The first C++ view can return before its language service finishes attaching command filters.
  for ($attempt = 0; $attempt -lt 150; ++$attempt) {
    $available = Invoke-Ide { $dte.Commands.Item('Edit.GoToDeclaration').IsAvailable }
    if ($available) { break }
    Start-Sleep -Milliseconds 200
  }
  Invoke-Ide { $dte.ExecuteCommand('Edit.GoToDeclaration') }
  for ($attempt = 0; $attempt -lt 150; ++$attempt) {
    Start-Sleep -Milliseconds 200
    $active = Invoke-Ide { $dte.ActiveDocument }
    if ($null -ne $active -and $active.FullName -eq $Expected -and
        $active.Selection.Text -eq $Token -and $active.Selection.TopPoint.Line -eq $Line) {
      Write-Output "PASS native declaration -> $Expected ($Token) at live line $Line"
      return
    }
  }
  $actual = Invoke-Ide { $dte.ActiveDocument }
  throw "Native declaration failed: expected $Expected ($Token) at live line $Line; active=$($actual.FullName), selected=$($actual.Selection.Text), line=$($actual.Selection.TopPoint.Line)"
}

$dte = $null
[uint32]$ownedProcessId = 0
$existingIds = @(Get-Process devenv -ErrorAction SilentlyContinue | ForEach-Object Id)
try {
  $dte = New-Object -ComObject $ProgId
  $created = @(Get-CimInstance Win32_Process -Filter "Name='devenv.exe'" |
    Where-Object { $_.ProcessId -notin $existingIds -and $_.CommandLine -like '*devenv.exe* -Embedding' })
  if ($created.Count -ne 1) { throw 'Cannot uniquely identify the newly created automation host.' }
  $ownedProcessId = $created[0].ProcessId
  Invoke-Ide { $dte.MainWindow.Visible = $false; $dte.SuppressUI = $true }
  Invoke-Ide { $dte.Solution.Create($root, 'SerializerGeneratedHost'); $dte.Solution.SaveAs((Join-Path $root 'SerializerGeneratedHost.sln')) }
  foreach ($file in @($schema, $included)) {
    Invoke-Ide {
      $dte.ItemOperations.OpenFile($file, '{7651a700-06e5-11d1-8ebd-00a0c90f26ea}') | Out-Null
      $selection = $dte.ActiveDocument.Selection
      $selection.StartOfDocument($false)
      $selection.Insert("// unsaved host qualification`n")
    }
  }
  foreach ($file in $generated) {
    # Anchor actual declarations; generated diagnostics also contain the type's spelling.
    $match = [regex]::Match([IO.File]::ReadAllText($file), '(?im)^\s*(?:(?:public|export|declare|static|final|sealed|pub)\s+)*(?:class|struct|type)\s+(?<token>(?:host_?test_?)?envelope)\b[^\r\n]*[{:]').Groups['token']
    if (-not $match.Success) { throw "Missing actual envelope declaration in $file" }
    foreach ($reversed in @($false, $true)) {
      Select-Token $file $match.Value $reversed $match.Index
      Assert-Declaration $schema 'envelope' 4
    }
  }
  foreach ($reversed in @($false, $true)) {
    # The generated file's type declaration resolves through two source include levels.
    $file = $generated[0]
    $match = [regex]::Match([IO.File]::ReadAllText($file), '(?im)^class (?<token>item)\b').Groups['token']
    if (-not $match.Success) { throw 'Missing included item declaration.' }
    Select-Token $file $match.Value $reversed $match.Index
    Assert-Declaration $included 'item' 3
  }
  if ([IO.File]::ReadAllText($schema).Contains('unsaved') -or [IO.File]::ReadAllText($included).Contains('unsaved')) {
    throw 'Host navigation saved a source buffer.'
  }
  Write-Output 'PASS: 24 native Visual Studio checks, eleven actual outputs, reversed selections, transitive includes and live unsaved source offsets.'
} finally {
  if ($ownedProcessId -ne 0) { Stop-Process -Id $ownedProcessId -Force -ErrorAction SilentlyContinue }
}
