$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\scripts\c2-supervisor-invocation.ps1')

$missing = Invoke-C2SupervisorProcess `
    -Supervisor (Join-Path $PSScriptRoot 'definitely-missing-supervisor.exe') `
    -Arguments @()
if ($missing.LaunchSucceeded -or $null -ne $missing.ExitCode -or
    $missing.Output.Count -ne 1 -or $missing.Output[0] -ne 'supervisor_launch_failed') {
    throw 'Missing supervisor did not fail closed.'
}

$powershell = Join-Path $PSHOME 'powershell.exe'
$success = Invoke-C2SupervisorProcess -Supervisor $powershell `
    -Arguments @('-NoProfile', '-Command', 'exit 0')
if (-not $success.LaunchSucceeded -or $success.ExitCode -ne 0) {
    throw 'Successful native launch was not retained.'
}

$failure = Invoke-C2SupervisorProcess -Supervisor $env:ComSpec `
    -Arguments @('/d', '/c', 'echo typed_native_failure 1>&2 & exit /b 7')
if (-not $failure.LaunchSucceeded -or $failure.ExitCode -ne 7 -or
    @($failure.Output | ForEach-Object { ("$_").Trim() }) -notcontains
        'typed_native_failure') {
    throw 'Nonzero native launch evidence was not retained.'
}
