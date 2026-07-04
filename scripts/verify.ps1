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
    $global:LASTEXITCODE = 0
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

function Assert-ProbeResult {
    param(
        [Parameter(Mandatory = $true)][string]$SummaryJson,
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [Parameter(Mandatory = $true)][string]$ExpectedStatus,
        [string]$ExpectedFailureReason
    )

    if (-not (Test-Path -LiteralPath $ManifestPath)) {
        throw "Expected manifest was not written: $ManifestPath"
    }

    $summary = $SummaryJson | ConvertFrom-Json
    if ($summary.status -ne $ExpectedStatus) {
        throw "Expected probe status '$ExpectedStatus', got '$($summary.status)'"
    }
    if ($summary.failure_reason -ne $ExpectedFailureReason) {
        throw "Expected failure reason '$ExpectedFailureReason', got '$($summary.failure_reason)'"
    }

    $manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    if ($manifest.status -ne $ExpectedStatus) {
        throw "Expected manifest status '$ExpectedStatus', got '$($manifest.status)'"
    }
    if ($manifest.failure_reason -ne $ExpectedFailureReason) {
        throw "Expected manifest failure reason '$ExpectedFailureReason', got '$($manifest.failure_reason)'"
    }
}

function Invoke-ProbeExpectSuccess {
    param(
        [Parameter(Mandatory = $true)][string]$ProbeExe,
        [Parameter(Mandatory = $true)][string]$ValidatorExe,
        [Parameter(Mandatory = $true)][string]$Mode,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$TelemetryPath,
        [Parameter(Mandatory = $true)][string]$ManifestPath
    )

    $summaryJson = & $ProbeExe --mode $Mode --run-id $RunId --telemetry $TelemetryPath --manifest $ManifestPath
    if ($LASTEXITCODE -ne 0) {
        throw "Probe unexpectedly failed with exit code $LASTEXITCODE"
    }
    Assert-ProbeResult $summaryJson $ManifestPath "ok" ""
    & $ValidatorExe $TelemetryPath
    if ($LASTEXITCODE -ne 0) {
        throw "Lifecycle validation failed for $TelemetryPath"
    }
}

function Invoke-ProbeExpectFailure {
    param(
        [Parameter(Mandatory = $true)][string]$ProbeExe,
        [Parameter(Mandatory = $true)][string]$ValidatorExe,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$TelemetryPath,
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [Parameter(Mandatory = $true)][string]$SimulationFlag,
        [Parameter(Mandatory = $true)][string]$SimulationValue,
        [Parameter(Mandatory = $true)][string]$ExpectedFailureReason
    )

    $summaryJson = & $ProbeExe --mode 1gb-safe-cpu --run-id $RunId --telemetry $TelemetryPath --manifest $ManifestPath $SimulationFlag $SimulationValue
    if ($LASTEXITCODE -eq 0) {
        throw "Probe unexpectedly succeeded for $ExpectedFailureReason"
    }
    Assert-ProbeResult $summaryJson $ManifestPath "failed_closed" $ExpectedFailureReason
    & $ValidatorExe $TelemetryPath
    if ($LASTEXITCODE -ne 0) {
        throw "Lifecycle validation failed for $TelemetryPath"
    }
}

function Remove-ProbeArtifacts {
    param([Parameter(Mandatory = $true)][string[]]$Paths)
    Remove-Item -LiteralPath $Paths -Force -ErrorAction SilentlyContinue
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
            Invoke-ProbeExpectSuccess ".\build\Debug\prism-probe.exe" ".\build\Debug\prism-validate-lifecycle.exe" "1gb-safe-cpu" "verify-cpu-smoke" "verify-cpu-smoke.jsonl" "verify-cpu-smoke-manifest.json"
            Remove-ProbeArtifacts @("verify-cpu-smoke.jsonl", "verify-cpu-smoke-manifest.json")
        }

        Run-Step "Forced breach smoke matrix" {
            $cases = @(
                @{ RunId = "verify-forced-allocator"; Telemetry = "verify-forced-allocator.jsonl"; Manifest = "verify-forced-allocator-manifest.json"; Flag = "--simulate-allocator-peak-bytes"; Value = "1073741825"; Reason = "allocator_peak_exceeded_cap" },
                @{ RunId = "verify-forced-process"; Telemetry = "verify-forced-process.jsonl"; Manifest = "verify-forced-process-manifest.json"; Flag = "--simulate-process-gpu-peak-bytes"; Value = "1073741825"; Reason = "process_gpu_peak_exceeded_cap" },
                @{ RunId = "verify-forced-warmup"; Telemetry = "verify-forced-warmup.jsonl"; Manifest = "verify-forced-warmup-manifest.json"; Flag = "--simulate-warmup-peak-bytes"; Value = "1073741825"; Reason = "warmup_peak_exceeded_cap" },
                @{ RunId = "verify-forced-unknown"; Telemetry = "verify-forced-unknown.jsonl"; Manifest = "verify-forced-unknown-manifest.json"; Flag = "--simulate-unknown-post-warmup-bytes"; Value = "1"; Reason = "unknown_post_warmup_allocation" }
            )

            foreach ($case in $cases) {
                Invoke-ProbeExpectFailure ".\build\Debug\prism-probe.exe" ".\build\Debug\prism-validate-lifecycle.exe" $case.RunId $case.Telemetry $case.Manifest $case.Flag $case.Value $case.Reason
                Remove-ProbeArtifacts @($case.Telemetry, $case.Manifest)
            }
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
                Invoke-ProbeExpectSuccess ".\build-cuda\Debug\prism-probe.exe" ".\build-cuda\Debug\prism-validate-lifecycle.exe" "1gb-safe-gpu-probed" "verify-gpu-smoke" "verify-gpu-smoke.jsonl" "verify-gpu-smoke-manifest.json"
                Remove-ProbeArtifacts @("verify-gpu-smoke.jsonl", "verify-gpu-smoke-manifest.json")
            }
        }
    }
} finally {
    Pop-Location
}
