[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [switch]$All
)

$ErrorActionPreference = "Stop"

Push-Location (Resolve-Path (Join-Path $PSScriptRoot ".."))
try {
    $paths = @("build", ".github-run-artifacts")
    $paths += Get-ChildItem -Path . -Directory -Filter "build-*" -ErrorAction SilentlyContinue |
        ForEach-Object { $_.FullName }

    foreach ($path in ($paths | Sort-Object -Unique)) {
        if (Test-Path $path) {
            if ($PSCmdlet.ShouldProcess($path, "Remove generated build directory")) {
                Remove-Item -LiteralPath $path -Recurse -Force
            }
        }
    }

    $generatedPatterns = @(
        "*.jsonl",
        "*-manifest.json",
        "prisminfer-manifest.json",
        "prisminfer-probe.jsonl",
        "phase4-90b-simulated-plan.json",
        "phase4-overclaim-rejected.json"
    )

    foreach ($pattern in $generatedPatterns) {
        Get-ChildItem -Path . -Filter $pattern -File -ErrorAction SilentlyContinue |
            ForEach-Object {
                if ($PSCmdlet.ShouldProcess($_.FullName, "Remove generated probe artifact")) {
                    Remove-Item -LiteralPath $_.FullName -Force
                }
            }
    }

    if ($All) {
        Get-ChildItem -Path . -Directory -Force -Filter ".vs" -ErrorAction SilentlyContinue |
            ForEach-Object {
                if ($PSCmdlet.ShouldProcess($_.FullName, "Remove Visual Studio local state")) {
                    Remove-Item -LiteralPath $_.FullName -Recurse -Force
                }
            }
    }
} finally {
    Pop-Location
}
