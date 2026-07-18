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

    [ValidateRange(1, 67108864)]
    [UInt64]$PayloadBytes = 67108864,

    [ValidateRange(1, 999)]
    [int]$CudaArchitecture = 120,

    [string]$OutputRoot = 'c2-clearance-output'
)

$ErrorActionPreference = 'Stop'
$mutex = $null
$hasMutex = $false

try {
    if (-not $env:GITHUB_ACTIONS -or $env:GITHUB_ACTIONS -ne 'true') {
        throw 'The C2 hardware runner is restricted to an attended GitHub Actions dispatch.'
    }
    if ($env:GITHUB_SHA -ne $ReviewedSha) {
        throw 'ReviewedSha does not match GITHUB_SHA.'
    }
    $head = (git rev-parse HEAD).Trim()
    $tree = (git rev-parse 'HEAD^{tree}').Trim()
    if ($LASTEXITCODE -ne 0 -or $head -ne $ReviewedSha -or $tree -ne $SourceTreeSha) {
        throw 'The checked-out commit or tree does not match the reviewed authorization packet.'
    }
    if (git status --porcelain) {
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
    foreach ($caseName in @('success', 'post-context-telemetry-loss', 'heartbeat-loss', 'watchdog-cancel')) {
        & $supervisor --worker $worker --output-root $resolvedOutput --case $caseName `
            --workflow-run-id $env:GITHUB_RUN_ID --adapter-index $AdapterIndex `
            --payload-bytes $PayloadBytes
        if ($LASTEXITCODE -ne 0) {
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
