param(
    [Parameter(Mandatory = $true)][string]$ArtifactRoot,
    [Parameter(Mandatory = $true)][string]$ProvenanceRoot,
    [Parameter(Mandatory = $true)][string]$InventoryRoot,
    [Parameter(Mandatory = $true)][string]$DecoderExecutable
)
$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$catalog = Join-Path $repoRoot "configs\model-cell-catalog.json"
$verifier = Join-Path $PSScriptRoot "verify-model-artifact.py"
$temp = Join-Path ([IO.Path]::GetTempPath()) ("prisminfer-artifact-negatives-" + [Guid]::NewGuid().ToString("N"))
[IO.Directory]::CreateDirectory($temp) | Out-Null

function Invoke-Verify(
    [string]$Provenance,
    [string]$Inventory,
    [string]$CatalogPath = $catalog,
    [string]$EligibilityPath = (Join-Path $repoRoot "configs\ggml-tensor-eligibility.json"),
    [string]$DecoderPath = $DecoderExecutable
) {
    $previous = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    try {
        & python $verifier --catalog $CatalogPath --artifact-root $ArtifactRoot `
            --provenance-root $Provenance --inventory-root $Inventory `
            --decoder-executable $DecoderPath `
            --eligibility $EligibilityPath *> $null
        return $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previous }
}

try {
    if ((Invoke-Verify $ProvenanceRoot $InventoryRoot) -ne 0) { throw "Valid artifact DAG was rejected." }
    $prov = Join-Path $temp "provenance"; $inv = Join-Path $temp "inventory"
    [IO.Directory]::CreateDirectory($prov) | Out-Null; [IO.Directory]::CreateDirectory($inv) | Out-Null
    Get-ChildItem -LiteralPath $ProvenanceRoot -File | ForEach-Object { [IO.File]::Copy($_.FullName, (Join-Path $prov $_.Name), $true) }
    Get-ChildItem -LiteralPath $InventoryRoot -File | ForEach-Object { [IO.File]::Copy($_.FullName, (Join-Path $inv $_.Name), $true) }

    $record = Join-Path $prov "artifact-record.json"
    [IO.File]::WriteAllText($record, ([IO.File]::ReadAllText($record).Replace(
        '"artifact_bytes": 4920739232', '"artifact_bytes": 4920739231')))
    if ((Invoke-Verify $prov $InventoryRoot) -eq 0) { throw "Tampered artifact record was accepted." }
    [IO.File]::Copy((Join-Path $ProvenanceRoot "artifact-record.json"), $record, $true)

    $inventory = Join-Path $inv "inventory.json"
    [IO.File]::WriteAllText($inventory, ([IO.File]::ReadAllText($inventory).Replace(
        '"tensor_count": 292', '"tensor_count": 291')))
    if ((Invoke-Verify $ProvenanceRoot $inv) -eq 0) { throw "Tampered inventory was accepted." }
    [IO.File]::Copy((Join-Path $InventoryRoot "inventory.json"), $inventory, $true)

    $slice = Join-Path $inv "q4_k-first.bin"
    $bytes = [IO.File]::ReadAllBytes($slice); $bytes[0] = $bytes[0] -bxor 1; [IO.File]::WriteAllBytes($slice, $bytes)
    if ((Invoke-Verify $ProvenanceRoot $inv) -eq 0) { throw "Tampered retained slice was accepted." }
    [IO.File]::Delete($slice)
    if ((Invoke-Verify $ProvenanceRoot $inv) -eq 0) { throw "Missing retained slice was accepted." }

    $eligibility = Join-Path $temp "eligibility.json"
    [IO.File]::Copy((Join-Path $repoRoot "configs\ggml-tensor-eligibility.json"), $eligibility, $true)
    foreach ($mutation in @(
        @{ From = '"type_id": 12'; To = '"type_id": 13' },
        @{ From = '"block_bytes": 144'; To = '"block_bytes": 143' },
        @{ From = '"runtime_route": "pinned-upstream-fallback"'; To = '"runtime_route": "custom"' },
        @{ From = '"parser_semantics_revision": "13f2b28b098623391b1aacfd27995e1c8b7de9a9"'; To = '"parser_semantics_revision": "03f2b28b098623391b1aacfd27995e1c8b7de9a9"' }
    )) {
        $text = [IO.File]::ReadAllText((Join-Path $repoRoot "configs\ggml-tensor-eligibility.json")).Replace($mutation.From, $mutation.To)
        [IO.File]::WriteAllText($eligibility, $text)
        if ((Invoke-Verify $ProvenanceRoot $InventoryRoot $catalog $eligibility) -eq 0) { throw "Tampered eligibility was accepted: $($mutation.From)" }
    }

    $badCatalog = Join-Path $temp "catalog.json"
    [IO.File]::WriteAllText($badCatalog, ([IO.File]::ReadAllText($catalog).Replace(
        'a568f2ebc73cec3fd74ba2afd992d4e945a8c7a9d851f9b66163aac834b7b859',
        'a568f2e8c73cec3fd74ba2afd992d4e945a8c7a9d851f9b66163aac834b7b859')))
    if ((Invoke-Verify $ProvenanceRoot $InventoryRoot $badCatalog) -eq 0) { throw "Catalog use-policy mismatch was accepted." }

    $noOp = Join-Path $temp "no-op-decoder.cmd"
    $binding = (Get-Content -LiteralPath (Join-Path $repoRoot "configs\ggml-tensor-eligibility.json") -Raw | ConvertFrom-Json).decoder_build_binding_sha256
    [IO.File]::WriteAllText($noOp, "@echo off`r`nif `"%1`"==`"--identity`" echo packet-a-real-slice-decoder-v1:$binding`r`nexit /b 0`r`n")
    if ((Invoke-Verify $ProvenanceRoot $InventoryRoot $catalog `
            (Join-Path $repoRoot "configs\ggml-tensor-eligibility.json") $noOp) -eq 0) {
        throw "Unbound no-op decoder was accepted."
    }
}
finally {
    if ([IO.Directory]::Exists($temp)) { [IO.Directory]::Delete($temp, $true) }
}
Write-Output "Model artifact provenance negative tests PASS."
$global:LASTEXITCODE = 0
