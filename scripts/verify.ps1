param(
    [switch]$WithCuda,
    [switch]$SkipProbeSmoke
)

$ErrorActionPreference = "Stop"

function Refresh-Path {
    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machinePath;$userPath"
}

function Run-Step {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Command
    )

    Write-Host ""
    Write-Host "==> $Name"
    & $Command
}

Push-Location (Resolve-Path (Join-Path $PSScriptRoot ".."))
try {
    Refresh-Path

    Run-Step "Git diff whitespace check" {
        git diff --check
    }

    Run-Step "Workflow lint" {
        $actionlint = Get-Command actionlint -ErrorAction SilentlyContinue
        if (-not $actionlint) {
            throw "actionlint is required for workflow linting. Run scripts\dev-setup.ps1 -InstallMissing."
        }

        $workflowFiles = Get-ChildItem .github\workflows\*.yml -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }
        if ($workflowFiles) {
            actionlint -config-file actionlint.yaml @workflowFiles
        }
    }

    Run-Step "Configure default build" {
        cmake -S . -B build
    }

    Run-Step "Build default Debug" {
        cmake --build build --config Debug --parallel
    }

    Run-Step "CTest default Debug" {
        ctest --test-dir build -C Debug --output-on-failure
    }

    if (-not $SkipProbeSmoke) {
        Run-Step "CPU probe smoke" {
            .\build\Debug\prism-probe.exe --mode 1gb-safe-cpu --run-id verify-cpu-smoke --telemetry verify-cpu-smoke.jsonl --manifest verify-cpu-smoke-manifest.json
            .\build\Debug\prism-validate-lifecycle.exe verify-cpu-smoke.jsonl
            Remove-Item -LiteralPath verify-cpu-smoke.jsonl, verify-cpu-smoke-manifest.json -Force -ErrorAction SilentlyContinue
        }
    }

    if ($WithCuda) {
        Run-Step "Configure CUDA probe build" {
            cmake -S . -B build-cuda -DPRISMINFER_ENABLE_CUDA_PROBE=ON
        }

        Run-Step "Build CUDA probe Debug" {
            cmake --build build-cuda --config Debug --parallel
        }

        Run-Step "CTest CUDA probe Debug" {
            ctest --test-dir build-cuda -C Debug --output-on-failure
        }

        if (-not $SkipProbeSmoke) {
            Run-Step "CUDA GPU-probed smoke" {
                .\build-cuda\Debug\prism-probe.exe --mode 1gb-safe-gpu-probed --run-id verify-gpu-smoke --telemetry verify-gpu-smoke.jsonl --manifest verify-gpu-smoke-manifest.json
                .\build-cuda\Debug\prism-validate-lifecycle.exe verify-gpu-smoke.jsonl
                Remove-Item -LiteralPath verify-gpu-smoke.jsonl, verify-gpu-smoke-manifest.json -Force -ErrorAction SilentlyContinue
            }
        }
    }
} finally {
    Pop-Location
}
