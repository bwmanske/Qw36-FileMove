<#
.SYNOPSIS
    Build script for FileMove v1.3.0

.DESCRIPTION
    Configures and builds the FileMove project using CMake.
    Supports Release and Debug configurations with clean build options.

.PARAMETER Config
    Build configuration: Release (default) or Debug.

.PARAMETER Clean
    Removes the build directory before configuring and building.

.PARAMETER Both
    Builds both Release and Debug configurations.

.PARAMETER Test
    Runs the unit test harness after building.

.PARAMETER RC
    Regenerates the .rc resource script from the template and asset files.
    Useful after modifying asset files (icons, images) without a full rebuild.

.PARAMETER Help
    Displays usage information and exits.

.EXAMPLE
    .\build.ps1
    Builds Release configuration (default).

.EXAMPLE
    .\build.ps1 -Config Debug
    Builds Debug configuration.

.EXAMPLE
    .\build.ps1 -Clean
    Removes build directory and builds Release from scratch.

.EXAMPLE
    .\build.ps1 -Both
    Builds both Release and Debug configurations.

.EXAMPLE
    .\build.ps1 -Both -Clean
    Clean builds both Release and Debug configurations.

.EXAMPLE
    .\build.ps1 -Config Release -Test
    Builds Release and runs unit tests.

.EXAMPLE
    .\build.ps1 -RC
    Regenerates the .rc resource script from asset files.

.EXAMPLE
    .\build.ps1 -Help
    Displays help information.
#>

param(
    [Parameter(Position = 0)]
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",

    [switch]$Clean,
    [switch]$Both,
    [switch]$Test,
    [switch]$RC,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Show-Help {
    Write-Host ""
    Write-Host "FileMove Build Script" -ForegroundColor Cyan
    Write-Host "=====================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "USAGE:" -ForegroundColor Yellow
    Write-Host "  .\build.ps1 [-Config <Release|Debug>] [-Clean] [-Both] [-Test] [-RC] [-Help]"
    Write-Host ""
    Write-Host "OPTIONS:" -ForegroundColor Yellow
    Write-Host "  -Config    Build configuration: Release (default) or Debug"
    Write-Host "  -Clean     Remove build directory before configuring and building"
    Write-Host "  -Both      Build both Release and Debug configurations"
    Write-Host "  -Test      Run unit tests after building"
    Write-Host "  -RC        Regenerate .rc resource script from asset files"
    Write-Host "  -Help      Show this help message and exit"
    Write-Host ""
    Write-Host "EXAMPLES:" -ForegroundColor Yellow
    Write-Host "  .\build.ps1              Build Release (default)"
    Write-Host "  .\build.ps1 -Config Debug  Build Debug"
    Write-Host "  .\build.ps1 -Clean         Clean build of Release"
    Write-Host "  .\build.ps1 -Both          Build both Release and Debug"
    Write-Host "  .\build.ps1 -Both -Clean   Clean build of both"
    Write-Host "  .\build.ps1 -Test          Build Release and run tests"
    Write-Host "  .\build.ps1 -RC            Regenerate .rc resource script"
    Write-Host ""
    exit 0
}

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host ">> $Message" -ForegroundColor Green
}

function Write-ErrorAndExit {
    param([string]$Message)
    Write-Host ""
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 1
}

function Test-CMake {
    try {
        $null = cmake --version
    } catch {
        Write-ErrorAndExit "CMake is not installed or not in PATH. Install CMake 3.10+ and try again."
    }
}

function Test-MSBuild {
    # Try direct msbuild first (works in Developer PowerShell)
    try {
        $null = msbuild -version
        return
    } catch {
        # Fall through to vswhere lookup
    }

    # Locate MSBuild via vswhere
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($installPath) {
            $msbuildPath = Join-Path $installPath "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $msbuildPath) {
                $env:Path = "$msbuildPath;$env:Path"
                return
            }
        }
    }

    Write-ErrorAndExit "MSBuild is not available. Install Visual Studio Build Tools with C++ workload and try again."
}

function Invoke-Clean {
    $buildDir = Join-Path $PSScriptRoot "build"
    if (Test-Path $buildDir) {
        Write-Step "Removing build directory: $buildDir"
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }
}

function Invoke-Configure {
    param([string]$Cfg)
    Write-Step "Configuring CMake for $Cfg..."
    cmake -B build -A x64
    if ($LASTEXITCODE -ne 0) {
        Write-ErrorAndExit "CMake configuration failed."
    }
}

function Invoke-Build {
    param([string]$Cfg)
    Write-Step "Building $Cfg configuration..."
    cmake --build build --config $Cfg
    if ($LASTEXITCODE -ne 0) {
        Write-ErrorAndExit "Build failed for $Cfg configuration."
    }
    Write-Host "  Output: build\$Cfg\FileMove.exe" -ForegroundColor Gray
}

function Invoke-Tests {
    param([string]$Cfg)
    Write-Step "Running unit tests ($Cfg)..."
    Push-Location build
    ctest -C $Cfg --output-on-failure
    $testResult = $LASTEXITCODE
    Pop-Location
    if ($testResult -ne 0) {
        Write-ErrorAndExit "Unit tests failed for $Cfg configuration."
    }
    Write-Host "  All tests passed." -ForegroundColor Gray
}

function Invoke-RC {
    Write-Step "Regenerating .rc resource script..."
    if (-not (Test-Path (Join-Path $PSScriptRoot "build"))) {
        Invoke-Configure
    }
    cmake --build build --target generate_rc
    if ($LASTEXITCODE -ne 0) {
        Write-ErrorAndExit ".rc regeneration failed."
    }
    Write-Host "  Generated: build/FileMove.rc" -ForegroundColor Gray
}

# --- Main ---

if ($Help) {
    Show-Help
}

Test-CMake
Test-MSBuild

if ($Clean) {
    Invoke-Clean
}

if (-not (Test-Path (Join-Path $PSScriptRoot "build"))) {
    Invoke-Configure
}

if ($RC) {
    Invoke-RC
    Write-Host ""
    Write-Host "Done." -ForegroundColor Green
    exit 0
}

if ($Both) {
    Invoke-Build "Release"
    Invoke-Build "Debug"
    if ($Test) {
        Invoke-Tests "Release"
    }
} else {
    Invoke-Build $Config
    if ($Test) {
        Invoke-Tests $Config
    }
}

Write-Host ""
Write-Host "Build complete." -ForegroundColor Green
