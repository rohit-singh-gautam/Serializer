<#
.SYNOPSIS
Configure, build, clean or rebuild Serializer, or run CMake tests.
.DESCRIPTION
Keep native target behavior and options synchronized with the Linux Makefile.
The all and rebuild targets build enabled CMake targets and both editor VSIX packages without
installing them. Extension packaging requires Windows, Node.js 22+, npm and Visual
Studio MSBuild. The configure and test targets operate on CMake targets only.
Clean is equivalent to git clean -fdx across this repository. It removes all
untracked and ignored files, including caches, dependencies, editor packages,
untracked source and local configuration. Tracked files and their edits remain.
Rebuild performs that repository cleanup before configuring and building again.
The extension target builds both editor VSIX packages without running CMake or cleanup.
.EXAMPLE
./make.ps1 all
.EXAMPLE
./make.ps1 test -Configuration Debug
.EXAMPLE
./make.ps1 clean
.EXAMPLE
./make.ps1 rebuild -Jobs 8
.EXAMPLE
./make.ps1 extension
.EXAMPLE
./make.ps1 all -CMakeArgs '-DSERIALIZER_BUILD_TESTS=OFF'
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('all', 'configure', 'test', 'clean', 'rebuild', 'extension')]
    [string]$Target = 'all',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',
    [string]$BuildDirectory,
    [ValidateRange(1, 1024)]
    [int]$Jobs = 4,
    [AllowEmptyString()]
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string[]]$CMakeArgs = @(),
    [ValidateNotNullOrEmpty()]
    [string]$CMakeCommand = 'cmake',
    [ValidateNotNullOrEmpty()]
    [string]$CTestCommand = 'ctest'
)

$ErrorActionPreference = 'Stop'

# Forward arguments without shell evaluation and stop on the first failed command.
function Invoke-BuildCommand {
    param([string]$Command, [string[]]$Arguments)
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE"
    }
}

# Verify the cleanup boundary, then remove only Git's untracked and ignored files.
function Invoke-RepositoryClean {
    $gitRoot = Invoke-BuildCommand 'git' @('-C', $PSScriptRoot, 'rev-parse', '--show-toplevel')
    $expectedRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
    $actualRoot = [System.IO.Path]::GetFullPath($gitRoot)
    if (-not [string]::Equals($expectedRoot, $actualRoot,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing to clean outside the repository containing make.ps1.'
    }
    Invoke-BuildCommand 'git' @('-C', $expectedRoot, 'clean', '-fdx')
}

if ($Target -eq 'extension') {
    & (Join-Path $PSScriptRoot 'editors/build.ps1')
    return
}

if ($Target -in @('clean', 'rebuild')) {
    Invoke-RepositoryClean
    if ($Target -eq 'clean') {
        return
    }
}

if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $PSScriptRoot "out/build/make-$Configuration"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path $PSScriptRoot $BuildDirectory
}

$configureArguments = @('-S', $PSScriptRoot, '-B', $BuildDirectory,
    "-DCMAKE_BUILD_TYPE=$Configuration")
if ($VcpkgRoot) {
    $toolchain = Join-Path $VcpkgRoot 'scripts/buildsystems/vcpkg.cmake'
    if (-not (Test-Path -LiteralPath $toolchain -PathType Leaf)) {
        throw "Cannot find the vcpkg toolchain: $toolchain"
    }
    $configureArguments += "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
}
Invoke-BuildCommand $CMakeCommand ($configureArguments + $CMakeArgs)
if ($Target -eq 'configure') {
    return
}
$buildArguments = @('--build', $BuildDirectory, '--config', $Configuration)
if ($Target -eq 'rebuild') {
    # CMake completes cleanup before starting compilation, even with parallel jobs.
    $buildArguments += '--clean-first'
}
Invoke-BuildCommand $CMakeCommand ($buildArguments + @('--parallel', "$Jobs"))
if ($Target -in @('all', 'rebuild')) {
    & (Join-Path $PSScriptRoot 'editors/build.ps1')
}
if ($Target -eq 'test') {
    Invoke-BuildCommand $CTestCommand @('--test-dir', $BuildDirectory,
        '--build-config', $Configuration, '--output-on-failure')
}
