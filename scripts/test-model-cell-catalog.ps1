$ErrorActionPreference = "Stop"
$validator = Join-Path $PSScriptRoot "validate-model-cell-catalog.ps1"
$catalogPath = Join-Path $PSScriptRoot "..\configs\model-cell-catalog.json"
$catalogText = Get-Content -LiteralPath $catalogPath -Raw
$tempRoot = Join-Path $env:TEMP ("prisminfer-model-cell-catalog-" + $PID)
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

function Assert-Rejected {
    param([string]$Name, [string]$Text)
    $path = Join-Path $tempRoot ($Name + ".json")
    Set-Content -LiteralPath $path -Value $Text -NoNewline
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $path *> $null
        $code = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previous }
    if ($code -eq 0) { throw "Catalog negative fixture was accepted: $Name" }
}

function New-CatalogFixture {
    param([scriptblock]$Mutate)
    $copy = $catalogText | ConvertFrom-Json
    & $Mutate $copy
    return ($copy | ConvertTo-Json -Depth 20)
}

function Assert-CheckedFileRejected {
    param([string]$Name, [string]$EligibilityPath, [string]$ProvenanceRoot,
          [string]$CheckedCatalogPath = $catalogPath)
    $previous = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    try {
        $arguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $validator,
                       "-CatalogPath", $CheckedCatalogPath)
        if ($EligibilityPath) { $arguments += @("-EligibilityPath", $EligibilityPath) }
        if ($ProvenanceRoot) { $arguments += @("-CheckedProvenanceRoot", $ProvenanceRoot) }
        & powershell @arguments *> $null
        $code = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previous }
    if ($code -eq 0) { throw "Checked-file negative fixture was accepted: $Name" }
}

try {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $catalogPath
    if ($LASTEXITCODE -ne 0) { throw "Valid catalog was rejected." }

    Assert-Rejected -Name "tampered-smoke" -Text ($catalogText.Replace(
        "7002dc2bd5b77fb496c00236843472589eb63a8121dfb3a5db474470faddfc34",
        "0000000000000000000000000000000000000000000000000000000000000000"))
    Assert-Rejected -Name "deferred-artifact-credit" -Text (New-CatalogFixture {
        param($c) $c.cells[3].artifact_sha256 = ('a' * 64)
    })
    Assert-Rejected -Name "scale-selection" -Text (New-CatalogFixture {
        param($c) $c.cells[4].source_revision = "premature-selection"
    })
    Assert-Rejected -Name "unknown-field" -Text (New-CatalogFixture {
        param($c) $c.cells[6] | Add-Member -NotePropertyName unexpected -NotePropertyValue $true
    })
    Assert-Rejected -Name "owner-mismatch" -Text (New-CatalogFixture {
        param($c) $c.cells[4].owner_issue = 97
    })
    Assert-Rejected -Name "unsupported-without-reason" -Text (New-CatalogFixture {
        param($c) $c.cells[2].unsupported_reason = ""
    })
    Assert-Rejected -Name "verified-missing-license" -Text (New-CatalogFixture {
        param($c) $c.cells[1].PSObject.Properties.Remove("license_sha256")
    })
    Assert-Rejected -Name "verified-missing-toolchain" -Text (New-CatalogFixture {
        param($c) $c.cells[1].PSObject.Properties.Remove("converter_revision")
    })

    $missingProvenance = New-CatalogFixture {
        param($c)
        $c.cells[1].PSObject.Properties.Remove("inventory_sha256")
    }
    Assert-Rejected -Name "verified-missing-provenance" -Text $missingProvenance

    $eligibility = Join-Path $tempRoot "eligibility-tampered.json"
    [IO.File]::WriteAllText($eligibility, ([IO.File]::ReadAllText(
        (Join-Path $PSScriptRoot "..\configs\ggml-tensor-eligibility.json")).Replace(
        '"type_id": 12', '"type_id": 13')))
    Assert-CheckedFileRejected -Name "eligibility-content" -EligibilityPath $eligibility

    $provenance = Join-Path $tempRoot "provenance"
    [IO.Directory]::CreateDirectory($provenance) | Out-Null
    $sourceProvenance = Join-Path $PSScriptRoot "..\configs\model-artifacts\llama-3.1-8b-instruct-q4-k-m"
    Get-ChildItem -LiteralPath $sourceProvenance -File | ForEach-Object {
        [IO.File]::Copy($_.FullName, (Join-Path $provenance $_.Name))
    }
    $receipt = Join-Path $provenance "quantization-receipt.json"
    [IO.File]::WriteAllText($receipt, ([IO.File]::ReadAllText($receipt).Replace(
        'bartowski/Meta-Llama-3.1-8B-Instruct-GGUF', 'untrusted/replacement')))
    Assert-CheckedFileRejected -Name "receipt-content" -ProvenanceRoot $provenance

    [IO.File]::Copy((Join-Path $sourceProvenance "quantization-receipt.json"), $receipt, $true)
    $sourcePath = Join-Path $provenance "source-manifest.json"
    $sourceObject = [IO.File]::ReadAllText($sourcePath) | ConvertFrom-Json
    ($sourceObject.files | Where-Object path -eq "tokenizer.json").sha256 = ('0' * 64)
    [IO.File]::WriteAllText($sourcePath, ($sourceObject | ConvertTo-Json -Depth 10))
    $sourceHash = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $recordPath = Join-Path $provenance "artifact-record.json"
    $recordObject = [IO.File]::ReadAllText($recordPath) | ConvertFrom-Json
    $recordObject.source_manifest_sha256 = $sourceHash
    [IO.File]::WriteAllText($recordPath, ($recordObject | ConvertTo-Json -Depth 10))
    $recordHash = (Get-FileHash -LiteralPath $recordPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $projectionCatalog = $catalogText | ConvertFrom-Json
    $projectionCatalog.cells[1].source_manifest_sha256 = $sourceHash
    $projectionCatalog.cells[1].artifact_record_sha256 = $recordHash
    $projectionCatalogPath = Join-Path $tempRoot "projection-catalog.json"
    [IO.File]::WriteAllText($projectionCatalogPath, ($projectionCatalog | ConvertTo-Json -Depth 20))
    Assert-CheckedFileRejected -Name "recomputed-source-projection" -ProvenanceRoot $provenance `
        -CheckedCatalogPath $projectionCatalogPath
}
finally {
    [IO.Directory]::Delete($tempRoot, $true)
}

Write-Output "Model-cell catalog negative tests PASS."
$global:LASTEXITCODE = 0
