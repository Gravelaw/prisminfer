$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$clearanceScript = Join-Path $repositoryRoot 'scripts\run-c2-clearance.ps1'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) (
    'prisminfer-c2-git-failure-{0}' -f [Guid]::NewGuid().ToString('N'))
$fakeBin = Join-Path $temporaryRoot 'bin'
$fakeGit = Join-Path $fakeBin 'git.cmd'
$originalPath = $env:Path

try {
    New-Item -ItemType Directory -Path $fakeBin | Out-Null
    @'
@echo off
>&2 echo fatal: detected dubious ownership in repository at '%CD%'
exit /b 128
'@ | Set-Content -LiteralPath $fakeGit -Encoding ascii

    $env:Path = '{0};{1}' -f $fakeBin, $originalPath
    $env:GITHUB_ACTIONS = 'true'
    $env:GITHUB_SHA = '1111111111111111111111111111111111111111'

    $ErrorActionPreference = 'Continue'
    try {
        $output = & (Join-Path $PSHOME 'powershell.exe') `
            -NoProfile `
            -ExecutionPolicy Bypass `
            -File $clearanceScript `
            -ReviewedSha $env:GITHUB_SHA `
            -SourceTreeSha '2222222222222222222222222222222222222222' `
            -AuthorizationId 'C2-GIT-FAILURE-TEST' `
            -GpuUuid 'GPU-11111111-2222-3333-4444-555555555555' 2>&1 | Out-String
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = 'Stop'
    }

    if ($exitCode -eq 0) {
        throw 'The clearance script unexpectedly accepted unavailable Git metadata.'
    }
    if ($output -notmatch 'Unable to read the checked-out commit for C2 authorization') {
        throw "The clearance script did not report the Git metadata failure: $output"
    }
    if ($output -match 'InvokeMethodOnNull|null-valued expression') {
        throw "The clearance script leaked an untyped null-method failure: $output"
    }
    Write-Host "C2 Git failure boundary PASS: exit code $exitCode, typed ownership guidance retained."
} finally {
    $env:Path = $originalPath
    Remove-Item Env:GITHUB_ACTIONS -ErrorAction SilentlyContinue
    Remove-Item Env:GITHUB_SHA -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
}
