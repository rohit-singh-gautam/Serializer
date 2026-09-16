<#
.SYNOPSIS
Configure and build Serializer, optionally running its tests.
.EXAMPLE
./make.ps1 all
.EXAMPLE
./make.ps1 test -Configuration Debug
.EXAMPLE
./make.ps1 all -CMakeArgs '-DSERIALIZER_BUILD_TESTS=OFF'
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('all', 'configure', 'test')]
    [string]$Target = 'all',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',
    [string]$BuildDirectory,
    [ValidateRange(1, 1024)]
    [int]$Jobs = 4,
    [AllowEmptyString()]
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string[]]$CMakeArgs = @()
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
Invoke-BuildCommand 'cmake' ($configureArguments + $CMakeArgs)
if ($Target -eq 'configure') {
    return
}
Invoke-BuildCommand 'cmake' @('--build', $BuildDirectory, '--config', $Configuration,
    '--parallel', "$Jobs")
if ($Target -eq 'test') {
    Invoke-BuildCommand 'ctest' @('--test-dir', $BuildDirectory,
        '--build-config', $Configuration, '--output-on-failure')
}
