param(
    [switch]$WithCuda,
    [switch]$WithCudaKernels,
    [string]$CudaArchs = "120",
    [ValidateRange(0, 8)][int]$BuildJobs = 0,
    [switch]$SkipProbeSmoke
)

$ErrorActionPreference = "Stop"

if ($env:OS -ne "Windows_NT") {
    throw "scripts/verify.ps1 is the Windows local verification path. Use the README CMake/CTest commands on other platforms."
}

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

function Get-HostMemorySnapshot {
    try {
        if ($env:OS -eq "Windows_NT") {
            $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
            $performance = Get-CimInstance Win32_PerfFormattedData_PerfOS_Memory -ErrorAction Stop
            return [pscustomobject]@{
                TotalBytes = [uint64]$os.TotalVisibleMemorySize * 1KB
                AvailableBytes = [uint64]$os.FreePhysicalMemory * 1KB
                CommitTotalBytes = [uint64]$performance.CommittedBytes
                CommitLimitBytes = [uint64]$performance.CommitLimit
            }
        }

    } catch {
        throw "Host memory telemetry failed; refusing automatic build parallelism: $($_.Exception.Message)"
    }
    throw "Host physical/commit telemetry is unavailable; refusing automatic build parallelism."
}

function Resolve-BuildJobs {
    param(
        [ValidateRange(0, 8)][int]$RequestedJobs,
        [AllowNull()][object]$MemorySnapshot = $null,
        [ValidateRange(1, 1024)][int]$ProcessorCount = [Environment]::ProcessorCount
    )

    $memory = if ($null -eq $MemorySnapshot) {
        Get-HostMemorySnapshot
    } else {
        $MemorySnapshot
    }

    [uint64]$totalBytes = $memory.TotalBytes
    [uint64]$availableBytes = $memory.AvailableBytes
    [uint64]$commitTotalBytes = $memory.CommitTotalBytes
    [uint64]$commitLimitBytes = $memory.CommitLimitBytes
    if ($totalBytes -eq 0 -or $availableBytes -gt $totalBytes -or
        $commitLimitBytes -eq 0 -or $commitTotalBytes -gt $commitLimitBytes) {
        throw "Host physical/commit telemetry is missing or contradictory."
    }

    [uint64]$physicalReserveBytes = [Math]::Max(
        [decimal]2GB, [Math]::Ceiling(([decimal]$totalBytes * 8) / 100))
    [uint64]$commitReserveBytes = [Math]::Max(
        [decimal]2GB, [Math]::Ceiling(([decimal]$commitLimitBytes * 5) / 100))
    [uint64]$commitHeadroomBytes = $commitLimitBytes - $commitTotalBytes
    if ($availableBytes -le $physicalReserveBytes -or
        $commitHeadroomBytes -le $commitReserveBytes) {
        throw "No build job fits the conservative T-101 development physical/commit preflight."
    }

    [uint64]$physicalPayloadBytes = $availableBytes - $physicalReserveBytes
    [uint64]$commitPayloadBytes = $commitHeadroomBytes - $commitReserveBytes
    [uint64]$safePayloadBytes = [Math]::Min(
        [decimal]$physicalPayloadBytes, [decimal]$commitPayloadBytes)
    # This is a conservative preflight estimate, not a process-tree memory cap
    # or live watchdog. Promotable resource enforcement remains owned by #103.
    [uint64]$conservativeBytesPerJob = 2GB
    [int]$memoryJobs = [Math]::Floor(
        [decimal]$safePayloadBytes / [decimal]$conservativeBytesPerJob)
    if ($memoryJobs -lt 1) {
        throw "No build job fits the conservative 2 GiB per-job envelope after T-101 reserves."
    }

    [int]$requestedCap = if ($RequestedJobs -gt 0) { $RequestedJobs } else { 8 }
    return [Math]::Min($requestedCap, [Math]::Min($ProcessorCount, $memoryJobs))
}

function Test-BuildJobPolicy {
    $base = [pscustomobject]@{
        TotalBytes = [uint64](32GB)
        AvailableBytes = [uint64](8GB)
        CommitTotalBytes = [uint64](16GB)
        CommitLimitBytes = [uint64](64GB)
    }
    if ((Resolve-BuildJobs -RequestedJobs 0 -MemorySnapshot $base -ProcessorCount 16) -ne 2) {
        throw "Build-job policy self-test failed for the 8 GiB development cell."
    }
    if ((Resolve-BuildJobs -RequestedJobs 8 -MemorySnapshot $base -ProcessorCount 16) -ne 2) {
        throw "Requested build jobs bypassed the safe memory-derived cap."
    }
    if ((Resolve-BuildJobs -RequestedJobs 1 -MemorySnapshot $base -ProcessorCount 16) -ne 1) {
        throw "A lower requested build-job cap was not preserved."
    }

    $twelveGiB = $base.PSObject.Copy()
    $twelveGiB.AvailableBytes = [uint64](12GB)
    if ((Resolve-BuildJobs -RequestedJobs 0 -MemorySnapshot $twelveGiB -ProcessorCount 16) -ne 4) {
        throw "Build-job policy self-test failed for the 12 GiB cell."
    }
    $fifteenGiB = $base.PSObject.Copy()
    $fifteenGiB.AvailableBytes = [uint64](15GB)
    if ((Resolve-BuildJobs -RequestedJobs 0 -MemorySnapshot $fifteenGiB -ProcessorCount 16) -ne 6) {
        throw "Build-job policy self-test failed for the 15 GiB cell."
    }

    [uint64]$physicalReserve = [Math]::Ceiling(([decimal](32GB) * 8) / 100)
    $oneJob = $base.PSObject.Copy()
    $oneJob.AvailableBytes = $physicalReserve + [uint64](2GB)
    if ((Resolve-BuildJobs -RequestedJobs 8 -MemorySnapshot $oneJob -ProcessorCount 16) -ne 1) {
        throw "Build-job policy failed at the exact one-job boundary."
    }

    $lowPhysical = $base.PSObject.Copy()
    $lowPhysical.AvailableBytes = [uint64](3GB)
    $lowPhysicalRejected = $false
    try {
        Resolve-BuildJobs -RequestedJobs 0 -MemorySnapshot $lowPhysical -ProcessorCount 16 | Out-Null
    } catch {
        $lowPhysicalRejected = $true
    }
    if (-not $lowPhysicalRejected) {
        throw "Build-job policy failed to reject insufficient physical payload."
    }

    $lowCommit = $base.PSObject.Copy()
    $lowCommit.CommitTotalBytes = [uint64](63GB)
    $lowCommitRejected = $false
    try {
        Resolve-BuildJobs -RequestedJobs 0 -MemorySnapshot $lowCommit -ProcessorCount 16 | Out-Null
    } catch {
        $lowCommitRejected = $true
    }
    if (-not $lowCommitRejected) {
        throw "Build-job policy failed to reject insufficient commit payload."
    }

    $contradictory = $base.PSObject.Copy()
    $contradictory.AvailableBytes = [uint64](33GB)
    $contradictoryRejected = $false
    try {
        Resolve-BuildJobs -RequestedJobs 0 -MemorySnapshot $contradictory -ProcessorCount 16 | Out-Null
    } catch {
        $contradictoryRejected = $true
    }
    if (-not $contradictoryRejected) {
        throw "Build-job policy failed to reject contradictory physical telemetry."
    }

    $missingCommit = $base.PSObject.Copy()
    $missingCommit.CommitLimitBytes = [uint64]0
    $missingCommitRejected = $false
    try {
        Resolve-BuildJobs -RequestedJobs 0 -MemorySnapshot $missingCommit -ProcessorCount 16 | Out-Null
    } catch {
        $missingCommitRejected = $true
    }
    if (-not $missingCommitRejected) {
        throw "Build-job policy failed to reject missing commit telemetry."
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

Test-BuildJobPolicy

Push-Location (Resolve-Path (Join-Path $PSScriptRoot ".."))
try {
    Refresh-Path

    Run-Step "Git diff whitespace check" {
        git diff --check
    }

    Run-Step "Model-cell catalog validation" {
        & (Join-Path $PSScriptRoot "validate-model-cell-catalog.ps1")
    }

    Run-Step "Model-cell catalog negative tests" {
        & (Join-Path $PSScriptRoot "test-model-cell-catalog.ps1")
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
        $defaultBuildJobs = Resolve-BuildJobs -RequestedJobs $BuildJobs
        Write-Host "Default build parallelism: $defaultBuildJobs (requested cap: $BuildJobs; 0 means automatic)"
        cmake --build build --config Debug --parallel $defaultBuildJobs
    }

    Run-Step "CTest default Debug" {
        ctest --test-dir build -C Debug --output-on-failure
    }

    Run-Step "Benchmark emitter reparse-point test" {
        & (Join-Path $PSScriptRoot "test-benchmark-emitter.ps1") -EmitterPath ".\build\Debug\prism-emit-benchmark.exe"
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
            $cudaBuildJobs = Resolve-BuildJobs -RequestedJobs $BuildJobs
            Write-Host "CUDA probe build parallelism: $cudaBuildJobs (requested cap: $BuildJobs; 0 means automatic)"
            cmake --build build-cuda --config Debug --parallel $cudaBuildJobs
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

    if ($WithCudaKernels) {
        if ([string]::IsNullOrWhiteSpace($CudaArchs)) {
            throw "-WithCudaKernels requires a non-empty -CudaArchs value"
        }

        Run-Step "Configure CUDA kernel build" {
            cmake -S . -B build/vs2026-cuda-sm120 -G "Visual Studio 18 2026" -A x64 -DPRISMINFER_ENABLE_CUDA_KERNELS=ON "-DPRISMINFER_CUDA_KERNEL_ARCHS=$CudaArchs"
        }

        Run-Step "Build CUDA kernel correctness test" {
            $kernelBuildJobs = Resolve-BuildJobs -RequestedJobs 1
            Write-Host "CUDA kernel build parallelism: $kernelBuildJobs (hard cap: 1)"
            cmake --build --preset vs2026-cuda-sm120 --parallel $kernelBuildJobs
        }

        Run-Step "CTest CUDA kernel correctness" {
            ctest --test-dir build/vs2026-cuda-sm120 -C Debug -L cuda_kernel --output-on-failure
        }
    }
} finally {
    Pop-Location
}
