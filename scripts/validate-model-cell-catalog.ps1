param(
    [string]$CatalogPath = (Join-Path $PSScriptRoot "..\configs\model-cell-catalog.json")
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$catalog = Get-Content -LiteralPath $CatalogPath -Raw | ConvertFrom-Json
if ($catalog.catalog_version -ne "0.1") {
    throw "Unsupported model-cell catalog version."
}
if ($catalog.catalog_status -notin @("metadata-only", "source-verified")) {
    throw "Unsupported model-cell catalog status."
}

$expected = @{
    "tiny-deterministic-smoke" = "smoke"
    "llama-3.1-8b-instruct-foundation" = "foundation"
    "ornith-1.0-9b-stress" = "stress"
    "qwen3.5-9b-lineage-comparator" = "lineage-comparator"
}
$seen = @{}
$shaPattern = '^[0-9a-fA-F]{64}$'
$expectedFoundation = @{
    Revision = "0e9e39f249a16976918f6564b8830bc894c89659"
    SourceManifestSha256 = "a5db8ce550b422ec693dbbf9180d554187d587b96407dfa67805349b55c1fe9d"
    SourceFileCount = 13
    SourceTotalBytes = [uint64]16069779031
    F16Sha256 = "3ff4b731b770599b1111cb141f9b6c0939ba93c920e7fc4ce45793da50431f61"
    F16Bytes = [uint64]16068896416
    PartialSha256 = "4577bdada251901e98184408522fc6bed20ed6a813ca304e13df185cdf688136"
    PartialBytes = [uint64]438796288
}

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

    if ($cell.status -eq "source-verified") {
        if ($cell.cell_id -ne "llama-3.1-8b-instruct-foundation" -or
            $cell.artifact_path -ne "" -or $cell.artifact_sha256 -ne "" -or
            $cell.source_revision -ne $expectedFoundation.Revision -or
            $cell.source_manifest_sha256 -ne $expectedFoundation.SourceManifestSha256 -or
            [uint64]$cell.source_file_count -ne $expectedFoundation.SourceFileCount -or
            [uint64]$cell.source_total_bytes -ne $expectedFoundation.SourceTotalBytes -or
            $cell.conversion_status -ne "q4-k-m-aborted-run-bound" -or
            $cell.f16_intermediate_sha256 -ne $expectedFoundation.F16Sha256 -or
            [uint64]$cell.f16_intermediate_bytes -ne $expectedFoundation.F16Bytes -or
            $cell.q4_k_m_partial_sha256 -ne $expectedFoundation.PartialSha256 -or
            [uint64]$cell.q4_k_m_partial_bytes -ne $expectedFoundation.PartialBytes -or
            $cell.q4_k_m_failure_reason -ne "run_bound_working_set_exceeded_4_gib" -or
            $cell.license_status -ne "accepted") {
            throw "Foundation source-verification record is incomplete or contradictory."
        }
    } elseif ($cell.status -eq "pinned") {
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

Write-Output "Model-cell catalog PASS: smoke fixture pinned; foundation source verified with aborted Q4_K_M record; stress and lineage remain unadmitted."
