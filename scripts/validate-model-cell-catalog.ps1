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
    SourceConfigSha256 = "29e4c210b0d6ac178b16b2a255a568bdb23b581e50ca1ef6a6d071dd85704e6e"
    TokenizerSha256 = "79e3e522635f3171300913bb421464a87de6222182a0570b9b2ccba2a964b2b4"
    TokenizerConfigSha256 = "177c7b61e616fecb84c17ce0591acb92c6c4d60e9ac5ababfb940ff23bbcd424"
    ChatTemplateSha256 = "e10ca381b1ccc5cf9db52e371f3b6651576caee0a630b452e2816b2d404d4b65"
    LicenseSha256 = "64e1b2889b7892e6bbe7a7ed5bfe6ff793c61f9d584345f8f41cf9f5cb30a369"
    UsePolicySha256 = "a568f2e8c73cec3fd74ba2afd992d4e945a8c7a9d851f9b66163aac834b7b859"
    ArchitectureFingerprint = "llama:LlamaForCausalLM;layers=32;hidden=4096;heads=32;kv_heads=8;intermediate=14336;vocab=128256;context=131072;rope=llama3"
    TokenizerClass = "PreTrainedTokenizerFast"
    ConverterRevision = "llama.cpp-13f2b28b098623391b1aacfd27995e1c8b7de9a9"
    ConversionCommand = "convert_hf_to_gguf.py --outtype f16 --use-temp-file"
    QuantizationRecipe = "Q4_K_M via llama-quantize with 2 CPU threads; all attempts non-promotable"
    ImatrixStatus = "not-used"
    F16Sha256 = "3ff4b731b770599b1111cb141f9b6c0939ba93c920e7fc4ce45793da50431f61"
    F16Bytes = [uint64]16068896416
    F16TensorInventorySha256 = "e9b535961d6df89bcfc5e815699bdbbbdb47a5c5a7a157a8c8f9005104d11c5a"
    F16TensorCount = 292
    F16TensorTypeCounts = "F16:226;F32:66"
    PartialSha256 = "4577bdada251901e98184408522fc6bed20ed6a813ca304e13df185cdf688136"
    PartialBytes = [uint64]438796288
    Q4Attempts = @(
        @{
            WorkingSetLimitBytes = [uint64]4294967296
            MaxWorkingSetBytes = [uint64]5297287168
            PartialSha256 = "4577bdada251901e98184408522fc6bed20ed6a813ca304e13df185cdf688136"
            PartialBytes = [uint64]438796288
            FailureReason = "run_bound_working_set_exceeded_4_gib"
        },
        @{
            WorkingSetLimitBytes = [uint64]6442450944
            MaxWorkingSetBytes = [uint64]6549295104
            PartialSha256 = "0916b758ec2e09ac8b08ee7a2a063e06fadfb7470da0061f30433b7f3910803c"
            PartialBytes = [uint64]807157760
            FailureReason = "run_bound_working_set_exceeded_6_gib"
        },
        @{
            WorkingSetLimitBytes = [uint64]8589934592
            MaxWorkingSetBytes = [uint64]8730607616
            PartialSha256 = "d501c642169d9ecd9aee76c0eceaae7c7fcf437521aa75bc40b5ff303b99e1cf"
            PartialBytes = [uint64]1469399040
            FailureReason = "run_bound_working_set_exceeded_8_gib"
        },
        @{
            WorkingSetLimitBytes = [uint64]10737418240
            MaxWorkingSetBytes = [uint64]11028516864
            PartialSha256 = "108fe36f86d7c4db08dc152270776091a44621b540cf4945d6a8086bf3963acc"
            PartialBytes = [uint64]2148466688
            FailureReason = "run_bound_working_set_exceeded_10_gib"
        },
        @{
            WorkingSetLimitBytes = [uint64]16106127360
            MaxWorkingSetBytes = [uint64]14082899968
            PartialSha256 = "a36429c67c0f874b8c58cf0c61c4933a3fc41e79ca85b7c0135c95d4db38089a"
            PartialBytes = [uint64]3039920128
            FailureReason = "physical_reserve_guard_triggered_unverified"
            RunReceiptSha256 = "74b0f14a0bc723b96c8d91843d581f26c83f6971b944807bd9e81a97b501ec81"
            DeclaredPhysicalReserveBytes = [uint64]4294967296
            DeclaredCommitReserveBytes = [uint64]6442450944
            TelemetryStatus = "missing_sampled_memory_values"
        }
    )
}
$expectedOrnith = @{
    Revision = "83dc1f5e24ef8527af019a6b3bf66ac0f1c2c999"
    SourceManifestSha256 = "d316380853635b07a3affde5698e96a2c3ffb1fd0ea20109cca3a9c4b1a54b89"
    SourceFileCount = 18
    SourceTotalBytes = [uint64]18848108671
    SourceConfigSha256 = "350e12e79ef582bdf6a2ff093930433b182e8736412ff227a4318826921565a5"
    TokenizerSha256 = "06b9509352d2af50381ab2247e083b80d32d5c0aba91c272ca9ff729b6a0e523"
    TokenizerConfigSha256 = "792fa3f0cb88b111e54ef3134c873531008c4df471d108da17903426e308aa7b"
    ChatTemplateSha256 = "8b4d21a1e70ccbc8849e5e5e6dd32f3acfb2e2060739475909edc6e9b27c7165"
    LicenseEvidencePath = "README.md front matter and model-card license statement"
    LicenseEvidenceSha256 = "fc6164ad80e63c2ea7e4f75d0694a3ebc095ef42537efe23ff3f4bddb6940b8a"
    ArchitectureFingerprint = "qwen3_5:Qwen3_5ForConditionalGeneration;text=qwen3_5_text;layers=32;hidden=4096;heads=16;kv_heads=4;vocab=248320"
    TokenizerClass = "Qwen2Tokenizer"
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
        if ($cell.cell_id -eq "llama-3.1-8b-instruct-foundation") {
        if ($cell.artifact_path -ne "" -or $cell.artifact_sha256 -ne "" -or
            $cell.source_revision -ne $expectedFoundation.Revision -or
            $cell.source_manifest_sha256 -ne $expectedFoundation.SourceManifestSha256 -or
            [uint64]$cell.source_file_count -ne $expectedFoundation.SourceFileCount -or
            [uint64]$cell.source_total_bytes -ne $expectedFoundation.SourceTotalBytes -or
            $cell.source_config_sha256 -ne $expectedFoundation.SourceConfigSha256 -or
            $cell.tokenizer_sha256 -ne $expectedFoundation.TokenizerSha256 -or
            $cell.tokenizer_config_sha256 -ne $expectedFoundation.TokenizerConfigSha256 -or
            $cell.chat_template_sha256 -ne $expectedFoundation.ChatTemplateSha256 -or
            $cell.license_sha256 -ne $expectedFoundation.LicenseSha256 -or
            $cell.use_policy_sha256 -ne $expectedFoundation.UsePolicySha256 -or
            $cell.architecture_fingerprint -ne $expectedFoundation.ArchitectureFingerprint -or
            $cell.tokenizer_class -ne $expectedFoundation.TokenizerClass -or
            $cell.converter_revision -ne $expectedFoundation.ConverterRevision -or
            $cell.conversion_command -ne $expectedFoundation.ConversionCommand -or
            $cell.quantization_recipe -ne $expectedFoundation.QuantizationRecipe -or
            $cell.imatrix_status -ne $expectedFoundation.ImatrixStatus -or
            $cell.conversion_status -ne "q4-k-m-aborted-run-bound" -or
            $cell.f16_intermediate_sha256 -ne $expectedFoundation.F16Sha256 -or
            [uint64]$cell.f16_intermediate_bytes -ne $expectedFoundation.F16Bytes -or
            $cell.f16_tensor_inventory_sha256 -ne $expectedFoundation.F16TensorInventorySha256 -or
            [uint64]$cell.f16_tensor_count -ne $expectedFoundation.F16TensorCount -or
            $cell.f16_tensor_type_counts -ne $expectedFoundation.F16TensorTypeCounts -or
            $cell.q4_k_m_partial_sha256 -ne $expectedFoundation.PartialSha256 -or
            [uint64]$cell.q4_k_m_partial_bytes -ne $expectedFoundation.PartialBytes -or
            $cell.q4_k_m_failure_reason -ne "run_bound_working_set_exceeded_4_gib" -or
            $cell.license_status -ne "accepted") {
            throw "Foundation source-verification record is incomplete or contradictory."
        }
        $attempts = @($cell.q4_k_m_attempts)
        if ($attempts.Count -ne $expectedFoundation.Q4Attempts.Count) {
            throw "Foundation Q4_K_M attempt receipt count is incomplete or contradictory."
        }
        for ($index = 0; $index -lt $attempts.Count; $index++) {
            $actual = $attempts[$index]
            $expectedAttempt = $expectedFoundation.Q4Attempts[$index]
            if ([uint64]$actual.working_set_limit_bytes -ne $expectedAttempt.WorkingSetLimitBytes -or
                [uint64]$actual.max_working_set_bytes -ne $expectedAttempt.MaxWorkingSetBytes -or
                $actual.partial_sha256 -ne $expectedAttempt.PartialSha256 -or
                [uint64]$actual.partial_bytes -ne $expectedAttempt.PartialBytes -or
                $actual.failure_reason -ne $expectedAttempt.FailureReason -or
                (($actual.failure_reason -like "run_bound_working_set_exceeded_*") -and
                    ([uint64]$actual.max_working_set_bytes -le [uint64]$actual.working_set_limit_bytes)) -or
                (($actual.failure_reason -eq "physical_reserve_guard_triggered_unverified") -and
                    ([uint64]$actual.max_working_set_bytes -gt [uint64]$actual.working_set_limit_bytes))) {
                throw "Foundation Q4_K_M attempt receipt is incomplete or contradictory."
            }
            if ($actual.failure_reason -eq "physical_reserve_guard_triggered_unverified" -and
                ($actual.run_receipt_sha256 -ne $expectedAttempt.RunReceiptSha256 -or
                 [uint64]$actual.declared_physical_reserve_bytes -ne $expectedAttempt.DeclaredPhysicalReserveBytes -or
                 [uint64]$actual.declared_commit_reserve_bytes -ne $expectedAttempt.DeclaredCommitReserveBytes -or
                 $actual.telemetry_status -ne $expectedAttempt.TelemetryStatus)) {
                throw "Foundation Q4_K_M unverified reserve-guard receipt is incomplete or contradictory."
            }
        }
        } elseif ($cell.cell_id -eq "ornith-1.0-9b-stress") {
            if ($cell.artifact_path -ne "" -or $cell.artifact_sha256 -ne "" -or
                $cell.source_revision -ne $expectedOrnith.Revision -or
                $cell.source_manifest_sha256 -ne $expectedOrnith.SourceManifestSha256 -or
                [uint64]$cell.source_file_count -ne $expectedOrnith.SourceFileCount -or
                [uint64]$cell.source_total_bytes -ne $expectedOrnith.SourceTotalBytes -or
                $cell.source_config_sha256 -ne $expectedOrnith.SourceConfigSha256 -or
                $cell.tokenizer_sha256 -ne $expectedOrnith.TokenizerSha256 -or
                $cell.tokenizer_config_sha256 -ne $expectedOrnith.TokenizerConfigSha256 -or
                $cell.chat_template_sha256 -ne $expectedOrnith.ChatTemplateSha256 -or
                $cell.license_evidence_path -ne $expectedOrnith.LicenseEvidencePath -or
                $cell.license_evidence_sha256 -ne $expectedOrnith.LicenseEvidenceSha256 -or
                $cell.architecture_fingerprint -ne $expectedOrnith.ArchitectureFingerprint -or
                $cell.tokenizer_class -ne $expectedOrnith.TokenizerClass -or
                $cell.license_status -ne "accepted") {
                throw "Ornith source-verification record is incomplete or contradictory."
            }
        } else {
            throw "Unexpected source-verified model cell: $($cell.cell_id)"
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

Write-Output "Model-cell catalog PASS: smoke fixture pinned; foundation and Ornith source verified; no canonical stress artifact admitted."
