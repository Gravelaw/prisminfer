param(
    [string]$CatalogPath = (Join-Path $PSScriptRoot "..\configs\model-cell-catalog.json")
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$catalog = Get-Content -LiteralPath $CatalogPath -Raw | ConvertFrom-Json
if ($catalog.catalog_version -ne "0.1") {
    throw "Unsupported model-cell catalog version."
}
if ($catalog.catalog_status -ne "metadata-only") {
    throw "The checked-in catalog must remain metadata-only until model access is authorized."
}

$expected = @{
    "tiny-deterministic-smoke" = "smoke"
    "llama-3.1-8b-instruct-foundation" = "foundation"
    "ornith-1.0-9b-stress" = "stress"
    "qwen3.5-9b-lineage-comparator" = "lineage-comparator"
}
$seen = @{}
$shaPattern = '^[0-9a-fA-F]{64}$'

function Resolve-TrustedArtifactPath {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )

    $rootItem = Get-Item -LiteralPath $Root -Force
    if (($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Repository root is a reparse point."
    }
    $resolved = [IO.Path]::GetFullPath((Join-Path $Root $RelativePath))
    $rootPrefix = $Root.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Artifact path escapes the repository: $RelativePath"
    }

    $relative = $resolved.Substring($Root.TrimEnd('\').Length).TrimStart('\')
    $currentPath = $Root.TrimEnd('\')
    foreach ($component in ($relative -split '[\\/]')) {
        if ($component -eq "") {
            continue
        }
        $currentPath = Join-Path $currentPath $component
        $item = Get-Item -LiteralPath $currentPath -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Artifact path contains a reparse point: $RelativePath"
        }
    }
    return $resolved
}

foreach ($cell in $catalog.cells) {
    if ($seen.ContainsKey($cell.cell_id)) {
        throw "Duplicate model-cell id: $($cell.cell_id)"
    }
    $seen[$cell.cell_id] = $true
    if (-not $expected.ContainsKey($cell.cell_id) -or
        $expected[$cell.cell_id] -ne $cell.role) {
        throw "Unexpected model-cell role: $($cell.cell_id)"
    }
    if ($cell.artifact_sha256 -ne "" -and
        $cell.artifact_sha256 -notmatch $shaPattern) {
        throw "Invalid artifact SHA-256: $($cell.cell_id)"
    }

    if ($cell.status -eq "pinned") {
        if ($cell.cell_id -ne "tiny-deterministic-smoke" -or
            $cell.artifact_kind -ne "synthetic-text" -or
            $cell.artifact_path -eq "" -or
            $cell.source_revision -eq "" -or
            $cell.artifact_sha256 -notmatch $shaPattern) {
            throw "Only the checked-in deterministic smoke fixture may be pinned."
        }
        $resolved = Resolve-TrustedArtifactPath -Root $repoRoot -RelativePath $cell.artifact_path
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw "Pinned artifact is missing: $resolved"
        }
        $actual = (Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $cell.artifact_sha256.ToLowerInvariant()) {
            throw "Pinned artifact hash mismatch: $($cell.cell_id)"
        }
    } else {
        if ($cell.artifact_path -ne "" -or $cell.artifact_sha256 -ne "" -or
            $cell.source_revision -ne "") {
            throw "Unadmitted model cells cannot carry local artifact identity: $($cell.cell_id)"
        }
    }
}

if ($seen.Count -ne $expected.Count) {
    throw "Model-cell catalog is missing one or more required metadata cells."
}

Write-Output "Model-cell catalog PASS: smoke artifact pinned; foundation/stress/lineage cells remain metadata-only and unadmitted."
