[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [Parameter(Mandatory = $true)][string[]]$ArgumentList,
    [Parameter(Mandatory = $true)][string]$ReceiptPath,
    [Parameter(Mandatory = $true)][string]$StdoutPath,
    [Parameter(Mandatory = $true)][string]$StderrPath,
    [uint64]$PhysicalReserveBytes = 4294967296,
    [uint64]$CommitReserveBytes = 6442450944,
    [uint64]$MaximumWorkingSetBytes = 16106127360,
    [uint32]$MaximumSeconds = 14400,
    [uint32]$SampleSeconds = 2
)

$ErrorActionPreference = "Stop"

function ConvertTo-NativeArgument {
    param([string]$Value)

    if ($Value -notmatch '[\s"]') { return $Value }
    $builder = [Text.StringBuilder]::new()
    [void]$builder.Append('"')
    $backslashes = 0
    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq '\') {
            ++$backslashes
        } elseif ($character -eq '"') {
            [void]$builder.Append(('\' * (($backslashes * 2) + 1)))
            [void]$builder.Append('"')
            $backslashes = 0
        } else {
            [void]$builder.Append(('\' * $backslashes))
            [void]$builder.Append($character)
            $backslashes = 0
        }
    }
    [void]$builder.Append(('\' * ($backslashes * 2)))
    [void]$builder.Append('"')
    return $builder.ToString()
}

function Get-MemorySample {
    param([System.Diagnostics.Process]$Process)

    $os = Get-CimInstance Win32_OperatingSystem
    $knownIds = [System.Collections.Generic.HashSet[uint32]]::new()
    [void]$knownIds.Add([uint32]$Process.Id)
    for ($depth = 0; $depth -lt 8; ++$depth) {
        $added = $false
        foreach ($candidate in @(Get-CimInstance Win32_Process)) {
            if ($knownIds.Contains([uint32]$candidate.ParentProcessId) -and
                $knownIds.Add([uint32]$candidate.ProcessId)) {
                $added = $true
            }
        }
        if (-not $added) { break }
    }

    [uint64]$workingSetBytes = 0
    [uint64]$privateBytes = 0
    foreach ($processId in $knownIds) {
        $member = Get-Process -Id $processId -ErrorAction SilentlyContinue
        if ($null -ne $member) {
            $workingSetBytes += [uint64]$member.WorkingSet64
            $privateBytes += [uint64]$member.PrivateMemorySize64
        }
    }
    return [ordered]@{
        sampled_at_utc = [DateTime]::UtcNow.ToString("o")
        physical_available_bytes = [uint64]$os.FreePhysicalMemory * 1024
        commit_available_bytes = [uint64]$os.FreeVirtualMemory * 1024
        process_tree_working_set_bytes = $workingSetBytes
        process_tree_private_bytes = $privateBytes
        process_tree_count = $knownIds.Count
    }
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$receiptParent = Split-Path -Parent $ReceiptPath
if (-not (Test-Path -LiteralPath $receiptParent -PathType Container)) {
    New-Item -ItemType Directory -Path $receiptParent -Force | Out-Null
}

$startedAt = [DateTime]::UtcNow
$samples = [System.Collections.Generic.List[object]]::new()
$stopReason = ""
$process = $null
$exitCode = $null

try {
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $resolvedExecutable
    $startInfo.Arguments = (($ArgumentList | ForEach-Object {
        ConvertTo-NativeArgument -Value $_
    }) -join ' ')
    $startInfo.WorkingDirectory = (Get-Location).Path
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) { throw "Native process did not start." }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()

    while (-not $process.HasExited) {
        $sample = Get-MemorySample -Process $process
        $samples.Add($sample)

        if ($sample.physical_available_bytes -lt $PhysicalReserveBytes) {
            $stopReason = "physical_reserve_guard_triggered"
        } elseif ($sample.commit_available_bytes -lt $CommitReserveBytes) {
            $stopReason = "commit_reserve_guard_triggered"
        } elseif ($sample.process_tree_working_set_bytes -gt $MaximumWorkingSetBytes) {
            $stopReason = "working_set_guard_triggered"
        } elseif (([DateTime]::UtcNow - $startedAt).TotalSeconds -gt $MaximumSeconds) {
            $stopReason = "deadline_guard_triggered"
        }

        if ($stopReason) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            $process.WaitForExit()
            break
        }

        Start-Sleep -Seconds $SampleSeconds
    }

    $process.WaitForExit()
    $exitCode = $process.ExitCode
    [IO.File]::WriteAllText($StdoutPath, $stdoutTask.Result,
        [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($StderrPath, $stderrTask.Result,
        [Text.UTF8Encoding]::new($false))
    if (-not $stopReason -and $exitCode -ne 0) {
        $stopReason = "process_exit_$exitCode"
    }
} catch {
    $stopReason = "runner_exception"
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
    throw
} finally {
    $completedAt = [DateTime]::UtcNow
    $receipt = [ordered]@{
        receipt_version = "packet-a-bounded-cpu-v1"
        executable = $resolvedExecutable
        arguments = $ArgumentList
        started_at_utc = $startedAt.ToString("o")
        completed_at_utc = $completedAt.ToString("o")
        duration_seconds = [math]::Round(($completedAt - $startedAt).TotalSeconds, 3)
        exit_code = $exitCode
        stop_reason = $stopReason
        limits = [ordered]@{
            physical_reserve_bytes = $PhysicalReserveBytes
            commit_reserve_bytes = $CommitReserveBytes
            maximum_working_set_bytes = $MaximumWorkingSetBytes
            maximum_seconds = $MaximumSeconds
            sample_seconds = $SampleSeconds
        }
        samples = $samples
    }
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReceiptPath -Encoding UTF8
}

if ($stopReason) {
    throw "Bounded CPU process failed closed: $stopReason"
}
