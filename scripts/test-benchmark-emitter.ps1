param(
    [Parameter(Mandatory = $true)][string]$EmitterPath
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $EmitterPath -PathType Leaf)) {
    throw "Benchmark emitter is missing: $EmitterPath"
}

$fixtureRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\tests\fixtures\kernel-manifests")).Path
$tempRoot = [IO.Path]::GetFullPath((Join-Path $env:TEMP ("prisminfer-emitter-" + [guid]::NewGuid().ToString("N"))))
$evidenceRoot = Join-Path $tempRoot "evidence"
$junction = Join-Path $evidenceRoot "outside"
New-Item -ItemType Directory -Force -Path $evidenceRoot | Out-Null
try {
    New-Item -ItemType Junction -Path $junction -Target $fixtureRoot | Out-Null
    $output = Join-Path $tempRoot "reparse-output.json"
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $EmitterPath (Join-Path $fixtureRoot "emitter-valid.json") $output `
            --output-root $tempRoot `
            --evidence-root $evidenceRoot --raw-trials (Join-Path $junction "raw-trials.jsonl") 2>&1 | Out-Null
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($exitCode -eq 0) {
        throw "Benchmark emitter accepted evidence through a junction."
    }
    if (Test-Path -LiteralPath $output) {
        throw "Benchmark emitter published output after rejecting a junction."
    }

    [IO.Directory]::Delete($junction, $false)

    $victim = Join-Path $tempRoot "victim.txt"
    [IO.File]::WriteAllText($victim, "victim-content")
    $hardlinkOutput = Join-Path $tempRoot "hardlink-output.json"
    New-Item -ItemType HardLink -Path $hardlinkOutput -Target $victim | Out-Null
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $EmitterPath (Join-Path $fixtureRoot "emitter-valid.json") $hardlinkOutput `
            --output-root $tempRoot `
            --evidence-root $fixtureRoot --raw-trials (Join-Path $fixtureRoot "raw-trials.jsonl") 2>&1 | Out-Null
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($exitCode -eq 0 -or [IO.File]::ReadAllText($victim) -ne "victim-content") {
        throw "Benchmark emitter overwrote an existing hard-linked output."
    }

    $sidecarOutput = Join-Path $tempRoot "sidecar-output.json"
    $sidecar = $sidecarOutput + ".sha256"
    New-Item -ItemType HardLink -Path $sidecar -Target $victim | Out-Null
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $EmitterPath (Join-Path $fixtureRoot "emitter-valid.json") $sidecarOutput `
            --output-root $tempRoot `
            --evidence-root $fixtureRoot --raw-trials (Join-Path $fixtureRoot "raw-trials.jsonl") 2>&1 | Out-Null
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($exitCode -eq 0 -or [IO.File]::ReadAllText($victim) -ne "victim-content") {
        throw "Benchmark emitter overwrote an existing hard-linked sidecar."
    }

    $legacyTempOutput = Join-Path $tempRoot "legacy-temp-output.json"
    New-Item -ItemType HardLink -Path ($legacyTempOutput + ".tmp") -Target $victim | Out-Null
    & $EmitterPath (Join-Path $fixtureRoot "emitter-valid.json") $legacyTempOutput `
        --output-root $tempRoot `
        --evidence-root $fixtureRoot --raw-trials (Join-Path $fixtureRoot "raw-trials.jsonl") | Out-Null
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText($victim) -ne "victim-content") {
        throw "Benchmark emitter retained the predictable temporary-path clobber behavior."
    }

    $outsideOutput = Join-Path ([IO.Path]::GetTempPath()) ("prisminfer-outside-" + [guid]::NewGuid().ToString("N") + ".json")
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $EmitterPath (Join-Path $fixtureRoot "emitter-valid.json") $outsideOutput `
            --output-root $tempRoot `
            --evidence-root $fixtureRoot --raw-trials (Join-Path $fixtureRoot "raw-trials.jsonl") 2>&1 | Out-Null
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($exitCode -eq 0 -or (Test-Path -LiteralPath $outsideOutput)) {
        throw "Benchmark emitter published outside the trusted output root."
    }
}
finally {
    if (Test-Path -LiteralPath $junction) {
        [IO.Directory]::Delete($junction, $false)
    }
    Remove-Item -LiteralPath $tempRoot -Force -Recurse -ErrorAction SilentlyContinue
}

Write-Output "Benchmark emitter filesystem-integrity tests PASS."
exit 0
