param(
    [string]$CatalogPath = (Join-Path $PSScriptRoot "..\configs\model-cell-catalog.json"),
    [string]$ArtifactRoot = "",
    [string]$ProvenanceRoot = "",
    [string]$InventoryRoot = "",
    [string]$DecoderExecutable = "",
    [string]$EligibilityPath = (Join-Path $PSScriptRoot "..\configs\ggml-tensor-eligibility.json"),
    [string]$CheckedProvenanceRoot = (Join-Path $PSScriptRoot "..\configs\model-artifacts\llama-3.1-8b-instruct-q4-k-m")
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$schemaPath = Join-Path $repoRoot "schemas\model_cell_catalog.schema.json"
$schemaValidator = Join-Path $PSScriptRoot "validate-json-schema.py"
& python $schemaValidator $schemaPath $CatalogPath
if ($LASTEXITCODE -ne 0) { throw "Model-cell catalog JSON Schema validation failed." }
$catalog = Get-Content -LiteralPath $CatalogPath -Raw | ConvertFrom-Json
$schema = Get-Content -LiteralPath $schemaPath -Raw | ConvertFrom-Json

if ($catalog.catalog_version -ne "0.2" -or
    $catalog.catalog_status -ne "artifact-descriptors-ready") {
    throw "Unsupported model-cell catalog version or status."
}

$expected = @{
    "tiny-deterministic-smoke" = @{ Role = "smoke"; State = "verified"; Owner = 80 }
    "llama-3.1-8b-instruct-foundation" = @{ Role = "foundation"; State = @("verified", "selection-deferred"); Owner = 80 }
    "ornith-1.0-9b-stress" = @{ Role = "stress"; State = "unsupported"; Owner = 80 }
    "qwen3.5-9b-lineage-comparator" = @{ Role = "lineage-comparator"; State = "selection-deferred"; Owner = 80 }
    "exact-30b-static-role" = @{ Role = "scale"; State = "selection-deferred"; Owner = 84 }
    "exact-70b-scale-role" = @{ Role = "scale"; State = "selection-deferred"; Owner = 97 }
    "exact-90b-scale-role" = @{ Role = "scale"; State = "selection-deferred"; Owner = 97 }
}
$allowedProperties = @($schema.'$defs'.cell.properties.PSObject.Properties.Name)
$shaPattern = '^[0-9a-f]{64}$'
$seen = @{}

function Get-OpenedFileSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
        [IO.FileShare]::Read)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
        finally { $sha.Dispose() }
    }
    finally { $stream.Dispose() }
}

function Resolve-TrustedLeaf {
    param([string]$Root, [string]$Relative)
    $rootItem = Get-Item -LiteralPath $Root -Force
    if (($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Artifact root is a reparse point."
    }
    $resolved = [IO.Path]::GetFullPath((Join-Path $Root $Relative))
    $prefix = $Root.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Artifact path escapes its trusted root."
    }
    $current = $Root.TrimEnd('\')
    foreach ($component in $resolved.Substring($current.Length).TrimStart('\') -split '[\\/]') {
        if (-not $component) { continue }
        $current = Join-Path $current $component
        $item = Get-Item -LiteralPath $current -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Artifact path contains a reparse point."
        }
    }
    return $resolved
}

foreach ($cell in @($catalog.cells)) {
    if ($seen.ContainsKey($cell.cell_id) -or -not $expected.ContainsKey($cell.cell_id)) {
        throw "Duplicate or unexpected model-cell id: $($cell.cell_id)"
    }
    $seen[$cell.cell_id] = $true
    $actualProperties = @($cell.PSObject.Properties.Name)
    if (@($actualProperties | Where-Object { $_ -notin $allowedProperties }).Count -ne 0) {
        throw "Unknown model-cell property: $($cell.cell_id)"
    }
    $rule = $expected[$cell.cell_id]
    if ($cell.role -ne $rule.Role -or [int]$cell.owner_issue -ne $rule.Owner -or
        $cell.artifact_state -notin @($rule.State)) {
        throw "Model-cell role/state/owner mismatch: $($cell.cell_id)"
    }
    if ($cell.artifact_sha256 -and $cell.artifact_sha256 -notmatch $shaPattern) {
        throw "Invalid artifact SHA-256: $($cell.cell_id)"
    }

    if ($cell.artifact_state -eq "verified") {
        if ($cell.artifact_sha256 -notmatch $shaPattern -or
            $cell.content_address -ne "artifact://sha256/$($cell.artifact_sha256)") {
            throw "Verified artifact identity mismatch: $($cell.cell_id)"
        }
        if ($cell.cell_id -eq "tiny-deterministic-smoke") {
            $leaf = Resolve-TrustedLeaf -Root $repoRoot -Relative $cell.artifact_path
            if ((Get-OpenedFileSha256 -Path $leaf) -ne $cell.artifact_sha256) {
                throw "Tiny smoke artifact hash mismatch."
            }
        } else {
            if ($cell.artifact_path -ne "" -or $cell.status -ne "verified" -or
                $cell.conversion_status -ne "q4-k-m-completed") {
                throw "External verified artifact carries invalid local state."
            }
            foreach ($field in @("artifact_record_sha256", "quantization_receipt_sha256",
                                  "inventory_sha256", "retained_slice_manifest_sha256", "eligibility_sha256")) {
                if ($cell.$field -notmatch $shaPattern) {
                    throw "Verified artifact provenance is incomplete: $field"
                }
            }
        }
    } else {
        if ($cell.artifact_path -ne "" -or $cell.artifact_sha256 -ne "" -or
            $cell.content_address -ne "") {
            throw "Unverified descriptor carries artifact credit: $($cell.cell_id)"
        }
        if ($cell.artifact_state -eq "unsupported" -and
            $cell.unsupported_reason -notin @("unsupported-converter", "not-admitted")) {
            throw "Unsupported descriptor lacks an immutable rejection reason: $($cell.cell_id)"
        }
    }

    if ($cell.role -eq "scale" -and
        ($cell.source_revision -ne "" -or $cell.artifact_kind -ne "descriptor-only")) {
        throw "Scale role prematurely selects an artifact: $($cell.cell_id)"
    }
}

if ($seen.Count -ne $expected.Count) {
    throw "Model-cell catalog is missing a required descriptor."
}

$llama = @($catalog.cells | Where-Object { $_.cell_id -eq "llama-3.1-8b-instruct-foundation" })[0]
$checkedFiles = @{
    eligibility_sha256 = @{ Path = $EligibilityPath; Schema = "ggml_tensor_eligibility.schema.json" }
    artifact_record_sha256 = @{ Path = (Join-Path $CheckedProvenanceRoot "artifact-record.json"); Schema = "model_artifact_record.schema.json" }
    quantization_receipt_sha256 = @{ Path = (Join-Path $CheckedProvenanceRoot "quantization-receipt.json"); Schema = "quantization_receipt.schema.json" }
    source_manifest_sha256 = @{ Path = (Join-Path $CheckedProvenanceRoot "source-manifest.json"); Schema = "source_manifest.schema.json" }
}
foreach ($field in $checkedFiles.Keys) {
    $entry = $checkedFiles[$field]
    & python $schemaValidator (Join-Path $repoRoot "schemas\$($entry.Schema)") $entry.Path
    if ($LASTEXITCODE -ne 0) { throw "Checked-in provenance schema failed: $field" }
    if ((Get-OpenedFileSha256 -Path $entry.Path) -ne $llama.$field) {
        throw "Checked-in provenance content address mismatch: $field"
    }
}
& python (Join-Path $PSScriptRoot "verify-model-artifact.py") --small-only `
    --catalog $CatalogPath --provenance-root $CheckedProvenanceRoot --eligibility $EligibilityPath
if ($LASTEXITCODE -ne 0) { throw "Checked-in provenance DAG validation failed." }

if ($ArtifactRoot) {
    if (-not $ProvenanceRoot -or -not $InventoryRoot -or -not $DecoderExecutable) {
        throw "ArtifactRoot verification requires explicit ProvenanceRoot, InventoryRoot, and DecoderExecutable."
    }
    & python (Join-Path $PSScriptRoot "verify-model-artifact.py") --catalog $CatalogPath `
        --artifact-root $ArtifactRoot --provenance-root $ProvenanceRoot --inventory-root $InventoryRoot `
        --decoder-executable $DecoderExecutable --eligibility $EligibilityPath
    if ($LASTEXITCODE -ne 0) { throw "External artifact DAG verification failed." }
}

Write-Output "Model-cell catalog PASS: immutable smoke, foundation/Ornith, and scale descriptors are structurally valid."
