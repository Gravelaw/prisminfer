param(
    [switch]$InstallMissing,
    [switch]$WithCuda,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Refresh-Path {
    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machinePath;$userPath"
}

function Find-Tool {
    param([Parameter(Mandatory = $true)][string]$Name)
    return Get-Command $Name -ErrorAction SilentlyContinue
}

function Install-WingetPackage {
    param(
        [Parameter(Mandatory = $true)][string]$Id,
        [Parameter(Mandatory = $true)][string]$Name
    )

    if (-not (Find-Tool winget)) {
        throw "$Name is missing and winget is not available for automatic install."
    }

    Write-Host "Installing $Name via winget..."
    winget install --id $Id --exact --silent --accept-package-agreements --accept-source-agreements
    Refresh-Path
}

function Require-Tool {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [string]$WingetId
    )

    $tool = Find-Tool $Name
    if ($tool) {
        Write-Host "OK: $Name -> $($tool.Source)"
        return
    }

    if ($InstallMissing -and $WingetId) {
        Install-WingetPackage -Id $WingetId -Name $Name
        $tool = Find-Tool $Name
        if ($tool) {
            Write-Host "OK: $Name -> $($tool.Source)"
            return
        }
    }

    throw "Missing required tool: $Name"
}

function Require-VisualStudioBuildTools {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe was not found. Install Visual Studio 2022 Build Tools with C++ workload."
    }

    $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installPath) {
        throw "Visual Studio C++ build tools were not found. Install the MSVC x64/x86 build tools component."
    }

    Write-Host "OK: Visual Studio Build Tools -> $installPath"
}

Push-Location (Resolve-Path (Join-Path $PSScriptRoot ".."))
try {
    Refresh-Path

    Require-Tool -Name git
    Require-Tool -Name gh
    Require-Tool -Name cmake
    Require-Tool -Name ctest
    Require-Tool -Name python
    Require-Tool -Name jq -WingetId "jqlang.jq"
    Require-Tool -Name actionlint -WingetId "rhysd.actionlint"
    Require-Tool -Name ninja -WingetId "Ninja-build.Ninja"
    Require-VisualStudioBuildTools

    if ($WithCuda) {
        Require-Tool -Name nvcc
        Require-Tool -Name nvidia-smi
    }

    if (-not $SkipBuild) {
        & "$PSScriptRoot\verify.ps1" -WithCuda:$WithCuda
    }
} finally {
    Pop-Location
}
