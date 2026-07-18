function Invoke-C2SupervisorProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Supervisor,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [string[]]$Arguments
    )

    if (-not (Test-Path -LiteralPath $Supervisor -PathType Leaf)) {
        return [pscustomobject]@{
            LaunchSucceeded = $false
            ExitCode = $null
            Output = @('supervisor_launch_failed')
        }
    }

    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $global:LASTEXITCODE = $null
    try {
        $output = @(& $Supervisor @Arguments 2>&1)
        $exitCode = $global:LASTEXITCODE
    } catch {
        $output = @("$_")
        $exitCode = $null
    } finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }

    return [pscustomobject]@{
        LaunchSucceeded = $null -ne $exitCode
        ExitCode = $exitCode
        Output = @($output)
    }
}
