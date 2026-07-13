$ErrorActionPreference = "Stop"

$validator = Join-Path $PSScriptRoot "validate-model-cell-catalog.ps1"
$catalogPath = Join-Path $PSScriptRoot "..\configs\model-cell-catalog.json"
$schemaPath = Join-Path $PSScriptRoot "..\schemas\model_cell_catalog.schema.json"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Remove-TestJunction {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    $item = Get-Item -LiteralPath $Path -Force
    $rootPrefix = $repoRoot.TrimEnd('\') + '\'
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0 -or
        -not $item.FullName.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove an unexpected test path: $Path"
    }
    [IO.Directory]::Delete($Path, $false)
}

function Test-Q4AttemptSchema {
    param([Parameter(Mandatory = $true)][string]$Path)

    $schema = Get-Content -LiteralPath $schemaPath -Raw | ConvertFrom-Json
    $itemSchema = $schema.'$defs'.cell.properties.q4_k_m_attempts.items
    $allowed = @($itemSchema.properties.PSObject.Properties.Name)
    $catalog = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    foreach ($cell in @($catalog.cells)) {
        foreach ($attempt in @($cell.q4_k_m_attempts)) {
            if ($null -eq $attempt) { continue }
            $actual = @($attempt.PSObject.Properties.Name)
            if (@($actual | Where-Object { $_ -notin $allowed }).Count -ne 0) { return $false }
            foreach ($required in @($itemSchema.required)) {
                if ($actual -notcontains $required) { return $false }
            }
            if ($attempt.failure_reason -eq "physical_reserve_guard_triggered_unverified") {
                foreach ($required in @($itemSchema.allOf[0].then.required)) {
                    if ($actual -notcontains $required) { return $false }
                }
            }
        }
    }
    return $true
}

if (-not (Test-Q4AttemptSchema -Path $catalogPath)) {
    throw "Valid model-cell catalog was rejected by its Q4_K_M receipt schema."
}

& powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $catalogPath
if ($LASTEXITCODE -ne 0) {
    throw "Valid model-cell catalog was rejected."
}

$tempRoot = [IO.Path]::GetFullPath((Join-Path $env:TEMP ("prisminfer-model-cell-catalog-" + $PID)))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
try {
    $catalogText = Get-Content -LiteralPath $catalogPath -Raw

    $badSchemaPath = Join-Path $tempRoot "bad-schema.json"
    $badSchemaText = $catalogText.Replace(
        '"telemetry_status": "missing_sampled_memory_values"',
        '"telemetry_status": "missing_sampled_memory_values", "unexpected_schema_field": true')
    Set-Content -LiteralPath $badSchemaPath -Value $badSchemaText -NoNewline
    if (Test-Q4AttemptSchema -Path $badSchemaPath) {
        throw "Catalog Q4_K_M receipt schema accepted an unrecognized field."
    }

    $badHashPath = Join-Path $tempRoot "bad-hash.json"
    $badHashText = $catalogText.Replace(
        "7002dc2bd5b77fb496c00236843472589eb63a8121dfb3a5db474470faddfc34",
        "0000000000000000000000000000000000000000000000000000000000000000")
    Set-Content -LiteralPath $badHashPath -Value $badHashText -NoNewline
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $badHashOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $badHashPath 2>&1
        $badHashExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($badHashExit -eq 0) {
        throw "Catalog validator accepted a mismatched smoke hash."
    }

    $badSourcePath = Join-Path $tempRoot "bad-source.json"
    $badSourceText = $catalogText.Replace(
        "a5db8ce550b422ec693dbbf9180d554187d587b96407dfa67805349b55c1fe9d",
        "0000000000000000000000000000000000000000000000000000000000000000")
    Set-Content -LiteralPath $badSourcePath -Value $badSourceText -NoNewline
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $badSourceOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $badSourcePath 2>&1
        $badSourceExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($badSourceExit -eq 0) {
        throw "Catalog validator accepted a mismatched foundation source manifest hash."
    }

    $badOrnithSourcePath = Join-Path $tempRoot "bad-ornith-source.json"
    $badOrnithSourceText = $catalogText.Replace(
        "d316380853635b07a3affde5698e96a2c3ffb1fd0ea20109cca3a9c4b1a54b89",
        "0000000000000000000000000000000000000000000000000000000000000000")
    Set-Content -LiteralPath $badOrnithSourcePath -Value $badOrnithSourceText -NoNewline
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $badOrnithSourceOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $badOrnithSourcePath 2>&1
        $badOrnithSourceExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($badOrnithSourceExit -eq 0) {
        throw "Catalog validator accepted a mismatched Ornith source manifest hash."
    }

    $badMetadataPath = Join-Path $tempRoot "bad-metadata.json"
    $badMetadataText = $catalogText.Replace(
        "29e4c210b0d6ac178b16b2a255a568bdb23b581e50ca1ef6a6d071dd85704e6e",
        "0000000000000000000000000000000000000000000000000000000000000000")
    Set-Content -LiteralPath $badMetadataPath -Value $badMetadataText -NoNewline
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $badMetadataOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $badMetadataPath 2>&1
        $badMetadataExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($badMetadataExit -eq 0) {
        throw "Catalog validator accepted a mismatched foundation source metadata hash."
    }

    $badInventoryPath = Join-Path $tempRoot "bad-inventory.json"
    $badInventoryText = $catalogText.Replace("F16:226;F32:66", "F16:225;F32:67")
    Set-Content -LiteralPath $badInventoryPath -Value $badInventoryText -NoNewline
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $badInventoryOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $badInventoryPath 2>&1
        $badInventoryExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($badInventoryExit -eq 0) {
        throw "Catalog validator accepted a mismatched F16 tensor inventory."
    }

    $badAttemptHashPath = Join-Path $tempRoot "bad-attempt-hash.json"
    $badAttemptHashText = $catalogText.Replace(
        "0916b758ec2e09ac8b08ee7a2a063e06fadfb7470da0061f30433b7f3910803c",
        "0000000000000000000000000000000000000000000000000000000000000000")
    Set-Content -LiteralPath $badAttemptHashPath -Value $badAttemptHashText -NoNewline
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $badAttemptHashOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $badAttemptHashPath 2>&1
        $badAttemptHashExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($badAttemptHashExit -eq 0) {
        throw "Catalog validator accepted a mismatched Q4_K_M attempt hash."
    }

    $badAttemptLimitPath = Join-Path $tempRoot "bad-attempt-limit.json"
    $badAttemptLimitText = $catalogText.Replace("6549295104", "6442450944")
    Set-Content -LiteralPath $badAttemptLimitPath -Value $badAttemptLimitText -NoNewline
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $badAttemptLimitOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $badAttemptLimitPath 2>&1
        $badAttemptLimitExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($badAttemptLimitExit -eq 0) {
        throw "Catalog validator accepted a Q4_K_M attempt that did not exceed its bound."
    }

    $badLatestAttemptHashPath = Join-Path $tempRoot "bad-latest-attempt-hash.json"
    $badLatestAttemptHashText = $catalogText.Replace(
        "d501c642169d9ecd9aee76c0eceaae7c7fcf437521aa75bc40b5ff303b99e1cf",
        "0000000000000000000000000000000000000000000000000000000000000000")
    Set-Content -LiteralPath $badLatestAttemptHashPath -Value $badLatestAttemptHashText -NoNewline
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $badLatestAttemptHashOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $badLatestAttemptHashPath 2>&1
        $badLatestAttemptHashExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($badLatestAttemptHashExit -eq 0) {
        throw "Catalog validator accepted a mismatched latest Q4_K_M attempt hash."
    }

    $badPath = Join-Path $tempRoot "bad-path.json"
    $badPathText = $catalogText.Replace(
        "tests/fixtures/model-cells/tiny-smoke-artifact.txt",
        "../outside-smoke-artifact.txt")
    Set-Content -LiteralPath $badPath -Value $badPathText -NoNewline
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $badPathOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $badPath 2>&1
        $badPathExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($badPathExit -eq 0) {
        throw "Catalog validator accepted an artifact path outside the repository."
    }

    $reparseLink = Join-Path $repoRoot "tests\fixtures\model-cells\.catalog-reparse-test"
    New-Item -ItemType Junction -Path $reparseLink -Target $env:TEMP -Force | Out-Null
    try {
        $reparsePath = Join-Path $tempRoot "bad-reparse.json"
        $reparseText = $catalogText.Replace(
            "tests/fixtures/model-cells/tiny-smoke-artifact.txt",
            "tests/fixtures/model-cells/.catalog-reparse-test/outside.txt")
        Set-Content -LiteralPath $reparsePath -Value $reparseText -NoNewline
        $savedPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            $reparseOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $validator -CatalogPath $reparsePath 2>&1
            $reparseExit = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $savedPreference
        }
        if ($reparseExit -eq 0) {
            throw "Catalog validator accepted an artifact path through a reparse point."
        }
    }
    finally {
        Remove-TestJunction -Path $reparseLink
    }
}
finally {
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-schema.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-hash.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-source.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-ornith-source.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-metadata.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-inventory.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-attempt-hash.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-attempt-limit.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-latest-attempt-hash.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-path.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $tempRoot "bad-reparse.json") -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $tempRoot -Force -ErrorAction SilentlyContinue
}

Write-Output "Model-cell catalog negative tests PASS."
exit 0
