#Requires -Version 5.1
<#
.SYNOPSIS
Exercise the installed extension's native commands in a new hidden Visual Studio instance.
.DESCRIPTION
Run with Windows PowerShell after building/installing the VSIX and generating the
VS Code test:navigation:generated fixtures. Uses a temporary solution and never
saves source documents. Only the automation process created here is terminated.
#>
param([string]$ProgId = 'VisualStudio.DTE.18.0')
$ErrorActionPreference = 'Stop'
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..')).Replace('\', '/')
$fixture = Get-ChildItem "$repository/out/extension-tests" -Directory -Filter 'generated-navigation-*' |
  Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($null -eq $fixture) { throw 'Run test:navigation:generated in editors/vscode first.' }
$root = "$repository/out/extension-tests/vs-host-$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $root -Force | Out-Null
$header = "$root/model.hpp"
Copy-Item -LiteralPath (Join-Path $fixture.FullName 'complex/model.hpp') -Destination $header
$schema = "$repository/example/schemas/complex/model.serializer"
[IO.File]::WriteAllText("$header.d", $header.Replace(':','\:').Replace(' ','\ ') + ': ' + $schema.Replace(':','\:').Replace(' ','\ ') + "`n")

# Retry rejected automation calls while the IDE initializes or dispatches commands.
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

# Select a complete token in either direction, reproducing native selection endpoints.
function Select-Token {
  param([string]$File, [string]$Token, [bool]$Reversed = $false)
  Invoke-Ide { $dte.ItemOperations.OpenFile($File, '{7651a700-06e5-11d1-8ebd-00a0c90f26ea}') } | Out-Null
  for ($attempt = 0; $attempt -lt 50; ++$attempt) {
    $document = $dte.ActiveDocument
    if ($null -ne $document -and $null -ne $document.Selection) { break }
    Start-Sleep -Milliseconds 200
  }
  $text = [IO.File]::ReadAllText($File)
  $offset = $text.IndexOf($Token, [StringComparison]::Ordinal)
  if ($offset -lt 0) { throw "Missing token $Token" }
  $prefix = $text.Substring(0, $offset)
  $line = ($prefix -split "`n").Count
  $column = $offset - $prefix.LastIndexOf("`n")
  Invoke-Ide {
    $selection = $document.Selection
    $selection.MoveToLineAndOffset($line, $column + $(if ($Reversed) { $Token.Length } else { 0 }), $false)
    $selection.MoveToLineAndOffset($line, $column + $(if ($Reversed) { 0 } else { $Token.Length }), $true)
  }
}

# Assert the actual native command's destination file and exact selected identifier.
function Assert-Navigation {
  param([string]$Command, [string]$Expected, [string]$Token)
  Invoke-Ide { $dte.ExecuteCommand($Command) }
  for ($attempt = 0; $attempt -lt 150; ++$attempt) {
    Start-Sleep -Milliseconds 200
    $active = Invoke-Ide { $dte.ActiveDocument.FullName }
    if ([IO.Path]::GetFullPath($active) -eq [IO.Path]::GetFullPath($Expected)) {
      $selected = Invoke-Ide { $dte.ActiveDocument.Selection.Text }
      if ($selected -eq $Token) { Write-Output "PASS $Command -> $Expected ($Token)"; return }
    }
  }
  throw "$Command failed: active=$active, expected=$Expected"
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
  Invoke-Ide { $dte.Solution.Create($root, 'SerializerNavigation'); $dte.Solution.SaveAs("$root/SerializerNavigation.sln") }
  foreach ($name in @('order', 'snapshot', 'customer')) {
    $relative = @{ order='sales/order'; snapshot='analytics/snapshot'; customer='people/customer' }[$name]
    foreach ($reversed in @($false, $true)) {
      Select-Token $schema "demo::$name" $reversed
      Assert-Navigation 'Edit.GoToDeclaration' "$repository/example/schemas/complex/$relative.serializer" $name
    }
  }
  Select-Token $schema 'demo::order'
  Assert-Navigation 'Edit.GoToDefinition' $header 'order'
  Assert-Navigation 'Edit.GoToDeclaration' "$repository/example/schemas/complex/sales/order.serializer" 'order'
  Write-Output 'Native Visual Studio declarations, reversed selections, generated definitions and reverse navigation passed.'
} finally {
  # DTE shutdown can wait on outstanding COM references; terminate only our disposable test host.
  if ($ownedProcessId -ne 0) { Stop-Process -Id $ownedProcessId -Force -ErrorAction SilentlyContinue }
}
