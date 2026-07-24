param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9a-f]{40}$')]
    [string]$ReviewedSha,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9a-f]{40}$')]
    [string]$SourceTreeSha,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9_.:-]{1,128}$')]
    [string]$AuthorizationId,

    [ValidateRange(0, 63)]
    [int]$AdapterIndex = 0,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^GPU-[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$')]
    [string]$GpuUuid,

    [ValidateRange(1, 67108864)]
    [UInt64]$PayloadBytes = 67108864,

    [ValidateRange(1, 999)]
    [int]$CudaArchitecture = 120,

    [string]$OutputRoot = 'c2-clearance-output'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'c2-supervisor-invocation.ps1')
$mutex = $null
$hasMutex = $false

function Get-C2GitText {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(git @Arguments 2>$null)
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($exitCode -ne 0) {
        throw "Unable to read $Description for C2 authorization (git exit code $exitCode). Run the attended runner and its workspace under the same Windows account; do not add a blanket safe.directory exception."
    }
    return (($lines | ForEach-Object { "$_" }) -join "`n").Trim()
}

try {
    if (-not $env:GITHUB_ACTIONS -or $env:GITHUB_ACTIONS -ne 'true') {
        throw 'The C2 hardware runner is restricted to an attended GitHub Actions dispatch.'
    }
    if ($env:GITHUB_SHA -ne $ReviewedSha) {
        throw 'ReviewedSha does not match GITHUB_SHA.'
    }
    $head = Get-C2GitText -Arguments @('rev-parse', 'HEAD') `
        -Description 'the checked-out commit'
    $tree = Get-C2GitText -Arguments @('rev-parse', 'HEAD^{tree}') `
        -Description 'the checked-out source tree'
    if ($head -ne $ReviewedSha -or $tree -ne $SourceTreeSha) {
        throw 'The checked-out commit or tree does not match the reviewed authorization packet.'
    }
    $status = Get-C2GitText -Arguments @('status', '--porcelain') `
        -Description 'the checkout cleanliness state'
    if ($status) {
        throw 'The reviewed-main checkout must be clean before C2 configuration.'
    }

    $mutex = [Threading.Mutex]::new($false, 'Global\PrismInfer.C2HardwareWorkflow')
    try {
        $hasMutex = $mutex.WaitOne(0)
    } catch [Threading.AbandonedMutexException] {
        throw 'The host-wide C2 lease was abandoned; preflight and cleanup must be repeated.'
    }
    if (-not $hasMutex) {
        throw 'Another PrismInfer hardware workflow owns the host-wide C2 lease.'
    }

    $resolvedOutput = [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputRoot))
    $workspace = [IO.Path]::GetFullPath((Get-Location))
    if (-not $resolvedOutput.StartsWith($workspace + [IO.Path]::DirectorySeparatorChar,
                                       [StringComparison]::OrdinalIgnoreCase)) {
        throw 'OutputRoot must resolve beneath the checked-out workspace.'
    }
    if (Test-Path -LiteralPath $resolvedOutput) {
        throw 'The C2 output directory already exists; stale evidence is not reusable.'
    }
    New-Item -ItemType Directory -Path $resolvedOutput | Out-Null

    $statusPath = Join-Path $resolvedOutput 'promotion-status.json'
    [ordered]@{
        schema_version = 'prisminfer-c2-promotion-status-v1'
        promotable = $false
        c2_credit = $false
        review_status = 'not-run'
        reason = 'workflow_incomplete'
        reviewed_sha = $ReviewedSha
        source_tree_sha = $SourceTreeSha
        authorization_id = $AuthorizationId
        workflow_run_id = $env:GITHUB_RUN_ID
    } | ConvertTo-Json -Compress | Set-Content -LiteralPath $statusPath -Encoding utf8

    $env:Path = '{0};{1}' -f [Environment]::GetEnvironmentVariable('Path', 'Machine'),
                              [Environment]::GetEnvironmentVariable('Path', 'User')
    cmake -S . -B build/c2-clearance -A x64 `
        -DPRISMINFER_ENABLE_CUDA_PROBE=ON `
        -DPRISMINFER_ENABLE_C2_SYNTHETIC=ON `
        "-DCMAKE_CUDA_ARCHITECTURES=$CudaArchitecture" `
        "-DPRISMINFER_C2_REVIEWED_SHA=$ReviewedSha" `
        "-DPRISMINFER_C2_SOURCE_TREE_SHA=$SourceTreeSha"
    if ($LASTEXITCODE -ne 0) { throw 'C2 configure failed.' }
    cmake --build build/c2-clearance --config Release --parallel 1 `
        --target prism-c2-supervisor prism-c2-synthetic-worker
    if ($LASTEXITCODE -ne 0) { throw 'C2 build failed.' }

    $supervisor = Join-Path $workspace 'build/c2-clearance/Release/prism-c2-supervisor.exe'
    $worker = Join-Path $workspace 'build/c2-clearance/Release/prism-c2-synthetic-worker.exe'
    if (-not (Test-Path -LiteralPath $supervisor -PathType Leaf) -or
        -not (Test-Path -LiteralPath $worker -PathType Leaf)) {
        throw 'C2 supervisor or worker build output is missing.'
    }
    foreach ($caseName in @('success', 'post-context-telemetry-loss', 'heartbeat-loss', 'watchdog-cancel')) {
        $arguments = @(
            '--worker', $worker,
            '--output-root', $resolvedOutput,
            '--case', $caseName,
            '--workflow-run-id', $env:GITHUB_RUN_ID,
            '--authorization-id', $AuthorizationId,
            '--gpu-uuid', $GpuUuid,
            '--adapter-index', "$AdapterIndex",
            '--payload-bytes', "$PayloadBytes"
        )
        $invocation = Invoke-C2SupervisorProcess -Supervisor $supervisor `
            -Arguments $arguments
        $caseOutput = @($invocation.Output | ForEach-Object { ("$_").Trim() })
        $caseExitCode = $invocation.ExitCode
        $caseOutput | ForEach-Object { Write-Host "$_" }
        if (-not $invocation.LaunchSucceeded -or $caseExitCode -ne 0) {
            $typedReason = @($caseOutput |
                    Where-Object { $_ -match '^[a-z0-9][a-z0-9_.:-]{0,255}$' } |
                    Select-Object -Last 1)
            if ($typedReason.Count -ne 1) {
                $typedReason = if ($invocation.LaunchSucceeded) {
                    @("supervisor_exit_$caseExitCode")
                } else {
                    @('supervisor_launch_failed')
                }
            }
            [ordered]@{
                schema_version = 'prisminfer-c2-promotion-status-v1'
                promotable = $false
                c2_credit = $false
                review_status = 'not-run'
                reason = $typedReason[0]
                reviewed_sha = $ReviewedSha
                source_tree_sha = $SourceTreeSha
                authorization_id = $AuthorizationId
                workflow_run_id = $env:GITHUB_RUN_ID
            } | ConvertTo-Json -Compress | Set-Content -LiteralPath $statusPath -Encoding utf8
            throw "C2 worker case '$caseName' failed; automatic retry is forbidden."
        }
    }

    [ordered]@{
        schema_version = 'prisminfer-c2-promotion-status-v1'
        promotable = $false
        c2_credit = $false
        review_status = 'pending-independent-review'
        reason = 'candidate_receipts_require_independent_acceptance'
        reviewed_sha = $ReviewedSha
        source_tree_sha = $SourceTreeSha
        authorization_id = $AuthorizationId
        workflow_run_id = $env:GITHUB_RUN_ID
    } | ConvertTo-Json -Compress | Set-Content -LiteralPath $statusPath -Encoding utf8
} finally {
    if ($hasMutex -and $mutex) {
        $mutex.ReleaseMutex()
    }
    if ($mutex) {
        $mutex.Dispose()
    }
}
