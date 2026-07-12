param(
    [Parameter(Mandatory = $true)][string]$EmitterPath
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $EmitterPath -PathType Leaf)) {
    throw "Benchmark emitter is missing: $EmitterPath"
}

$fixtureRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\tests\fixtures\kernel-manifests")).Path
$tempRoot = [IO.Path]::GetFullPath((Join-Path $env:TEMP ("prisminfer-emitter-" + $PID)))
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
}
finally {
    if (Test-Path -LiteralPath $junction) {
        [IO.Directory]::Delete($junction, $false)
    }
    Remove-Item -LiteralPath $tempRoot -Force -Recurse -ErrorAction SilentlyContinue
}

Write-Output "Benchmark emitter reparse-point test PASS."
exit 0
