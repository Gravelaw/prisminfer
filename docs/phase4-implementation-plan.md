# Phase 4 Implementation Plan: Large-Model and 90B Claim Validation

Phase 4 starts only after real backend governance, KV accounting, quality gates,
and transfer-inclusive profitability policy exist. Its purpose is to classify,
test, and possibly validate large-model hybrid/offload profiles without
overstating what the evidence proves.

## Phase 4 Goal

Move large-model profiles, including the terminal `>85B-90B` bucket, from
simulated planning to measured, validated, or rejected evidence under a strict
claim taxonomy and the current <=16 GiB GPU-cap envelope.

## Non-Goals

- No claim that dense 70B or 90B weights fully fit under the current 16 GiB
  maximum GPU cap.
- No deployable profile from extrapolation.
- No omission of host memory, IO, latency, or quality metrics.
- No committed model weights or large artifacts.
- No benchmark claim without exact model hash, quantization, hardware, and
  context length.

## Claim Taxonomy

Use these labels:

| Label | Meaning |
|---|---|
| `metadata-only` | Model/config/sidecar validation only. |
| `simulated` | Planner math or lower-bound/extrapolated result only. |
| `measured-offload` | Real run with model/hardware evidence, but not yet validated. |
| `validated-benchmark` | Cap, quality, latency, memory, and repeatability evidence pass. |
| `deployable-profile` | Validated benchmark plus documented operating constraints and repeatable deployment procedure. |
| `rejected` | A declared gate failed. |

The existing `90b-hybrid-simulated` profile remains non-deployable until a later
artifact proves otherwise.

## Architecture Direction

Phase 4 should add a planner and claim validator before any measured 90B run.

Proposed abstractions:

```cpp
struct HybridPlan {
  std::string model_id;
  std::string validation_cell_id;
  std::string model_parameter_bucket;
  std::uint64_t parameter_count;
  std::uint64_t tier_cap_bytes;
  std::uint64_t resident_weight_bytes;
  std::uint64_t weight_payload_bytes;
  std::uint64_t weight_metadata_bytes;
  std::uint64_t resident_kv_bytes;
  std::uint64_t kv_peak_bytes;
  std::uint64_t workspace_bytes;
  std::uint64_t runtime_overhead_bytes;
  std::uint64_t safety_margin_bytes;
  std::uint64_t gpu_budget_bytes;
  std::uint64_t host_budget_bytes;
  std::uint64_t nvme_budget_bytes;
  double expected_time_to_first_token_ms;
  double expected_decode_tokens_per_second;
};

struct ClaimEvidence {
  std::string claim_label;
  bool cap_passed;
  bool quality_passed;
  bool profitability_passed;
  bool repeatability_passed;
  std::string rejection_reason;
};
```

Initial tools:

- `prism-plan-90b`: dry-run planner, always simulated.
- `prism-validate-claim`: validates manifest/evidence completeness.
- measured large-model runs remain manual/self-hosted and external-artifact based.
- any plan requesting more than 16 GiB of GPU cap is rejected before
  classification.

## Lifecycle v0.5 Target

Phase 4 adds planning and claim events:

```text
hybrid_plan_created
claim_classified
usability_result
repeatability_result
claim_validation_result
```

Target sequence for 90B measured profiles:

```text
run_start
config_validated
telemetry_probe
model_sidecar_validated
backend_selected
dependency_pins_resolved
cuda_context_probe, if GPU requested
cap_semantics_resolved
hybrid_plan_created
claim_classified
host_prepare
model_load_plan
backend_init
backend_warmup
kv_cache_profile
prefill_start
prefill_end
decode_start
decode_end
quality_gate_result
profitability_result
usability_result
repeatability_result
claim_validation_result
memory_sample
cap_certification_result
completed | failed_closed
run_end
```

## Config and CLI Additions

Add config fields:

```json
{
  "claim_label": "metadata-only|simulated|measured-offload|validated-benchmark|deployable-profile",
  "parameter_count": 90000000000,
  "quantization_format": "",
  "quant_artifact_sha256": "",
  "host_memory_budget_bytes": 0,
  "nvme_budget_bytes": 0,
  "max_time_to_first_token_ms": 0,
  "min_decode_tokens_per_second": 0,
  "repeatability_runs": 3,
  "claim_validation": "fail-closed"
}
```

Add CLI flags:

```text
--claim-label LABEL
--parameter-count N
--quantization-format NAME
--quant-artifact-sha256 HASH
--host-memory-budget-bytes N
--nvme-budget-bytes N
--max-time-to-first-token-ms N
--min-decode-tokens-per-second R
--repeatability-runs N
--claim-validation off|warn|fail-closed
```

## Manifest v0.5 Additions

Record:

- claim label,
- model id and hash,
- quantization format and artifact hash,
- parameter count,
- model parameter bucket,
- validation cell id,
- shard list and hashes,
- GPU hardware and driver mode,
- CPU RAM profile,
- NVMe model/path/profile where available,
- context length and batch size,
- GPU cap,
- host memory peak,
- pinned host peak,
- NVMe bytes/token,
- time-to-first-token,
- sustained tokens/sec,
- p50/p95 token latency,
- quality gate result,
- profitability result,
- repeatability result,
- deployable flag,
- rejection reason.

## File and Module Candidates

New files:

```text
include/prisminfer/hybrid_plan.h
include/prisminfer/claim_taxonomy.h
include/prisminfer/usability_policy.h
include/prisminfer/evidence_bundle.h
src/planner/hybrid_plan.cpp
src/governor/claim_taxonomy.cpp
src/governor/usability_policy.cpp
src/validation/evidence_bundle.cpp
tools/prism-plan-90b/main.cpp
tools/prism-validate-claim/main.cpp
tests/unit/test_hybrid_plan.cpp
tests/unit/test_claim_taxonomy.cpp
tests/unit/test_usability_policy.cpp
configs/90b-hybrid-simulated.json
docs/claim-taxonomy.md
docs/phase4-90b-validation-policy.md
docs/phase4-evidence.md
```

Likely modified files:

```text
CMakeLists.txt
configs/90b-hybrid-offline.json
include/prisminfer/config.h
include/prisminfer/runtime_state.h
src/config.cpp
src/benchmark/manifest_writer.cpp
tools/prism-probe/main.cpp
tools/prism-validate-lifecycle/main.cpp
schemas/telemetry.schema.json
schemas/benchmark_manifest.schema.json
docs/risk-register.md
```

## Detailed Implementation Slices

### Planner Agent Slice Map

| Slice | GitHub issue candidate | File/module candidates | Gate |
|---|---|---|---|
| P4-01 | Add claim taxonomy enforcement | `include/prisminfer/claim_taxonomy.h`, `src/governor/claim_taxonomy.cpp`, `docs/claim-taxonomy.md` | Simulated evidence cannot be labeled deployable. |
| P4-02 | Add hybrid plan model | `include/prisminfer/hybrid_plan.h`, `src/planner/hybrid_plan.cpp` | GPU, host, NVMe, KV, transfer, and latency budgets are explicit. |
| P4-03 | Add simulated 90B planner tool | `tools/prism-plan-90b/main.cpp`, `configs/90b-hybrid-simulated.json` | Output is always labeled `simulated` unless measured evidence is supplied. |
| P4-04 | Extend manifest to lifecycle v0.5 | `schemas/benchmark_manifest.schema.json`, `schemas/telemetry.schema.json` | Model hash, quant hash, shards, hardware, host/NVMe profile, quality, and repeatability are recorded. |
| P4-05 | Add claim validation tool | `tools/prism-validate-claim/main.cpp` | Missing evidence fails closed for deployable or validated labels. |
| P4-06 | Add usability and repeatability policies | `include/prisminfer/usability_policy.h`, `src/governor/usability_policy.cpp` | Technically runnable but unusably slow runs are rejected. |
| P4-07 | Phase 4 evidence and audit | `docs/phase4-evidence.md`, `docs/risk-register.md` | 90B label is `validated-benchmark`, `deployable-profile`, `measured-offload`, `simulated`, or `rejected`. |

### P4-A: Evidence Bundle Schema

Purpose: make 90B claims validate against an evidence bundle instead of a loose
collection of benchmark files.

Add:

```text
include/prisminfer/evidence_bundle.h
src/validation/evidence_bundle.cpp
schemas/evidence_bundle.schema.json
tests/unit/test_evidence_bundle.cpp
```

Responsibilities:

- bind manifests, telemetry, model sidecars, dependency pins, quality results,
  profitability results, and repeatability results into one bundle,
- require hashes for model, quantization artifact, prompt set, and benchmark
  fixtures,
- keep simulated, measured, validated, and deployable labels mutually
  exclusive.

Acceptance:

- missing evidence prevents `validated-benchmark` and `deployable-profile`,
- simulated evidence cannot satisfy measured-offload requirements,
- schema validation works in hosted CI without large model artifacts.

### P4-B: 90B Planner and Claim Classifier

Purpose: separate mathematical feasibility planning from measured inference
claims.

Add or extend:

```text
include/prisminfer/hybrid_plan.h
include/prisminfer/claim_taxonomy.h
src/planner/hybrid_plan.cpp
src/governor/claim_taxonomy.cpp
tools/prism-plan-90b/main.cpp
tools/prism-classify-claim/main.cpp
tests/unit/test_hybrid_plan.cpp
tests/unit/test_claim_taxonomy.cpp
configs/90b-hybrid-validation-template.json
docs/phase4-90b-validation-policy.md
```

Responsibilities:

- estimate weight, KV, metadata, host, and NVMe budgets for candidate 90B
  profiles,
- reject requested GPU caps above 16 GiB,
- reject resident plans whose weight, metadata, KV, workspace, runtime overhead,
  and safety margin exceed the tier cap,
- mark planner output as `simulated` by default,
- classify evidence bundles into metadata-only, simulated, measured-offload,
  validated-benchmark, deployable-profile, or rejected,
- emit rejection reasons suitable for GitHub issue tracking.

Acceptance:

- `prism-plan-90b` never emits deployable output,
- impossible resident-memory math is rejected before claim classification,
- `prism-classify-claim` rejects overclassified claims,
- all claim labels are covered by unit tests.

### P4-C: Repeatability and Usability Gates

Purpose: reject technically runnable profiles that are too slow, too unstable,
or too incomplete to support a project claim.

Add or extend:

```text
include/prisminfer/usability_policy.h
src/governor/usability_policy.cpp
tests/unit/test_usability_policy.cpp
docs/phase4-evidence.md
```

Responsibilities:

- enforce maximum time-to-first-token,
- enforce minimum sustained decode tokens/sec,
- require repeated runs when claim label is validated or deployable,
- record p50/p95 latency and variance across runs.

Acceptance:

- missing repeatability data rejects validated claims,
- unusably slow measured-offload runs become `rejected`,
- deployable-profile requires a documented operating envelope.

## Issue Slices

Create a Phase 4 milestone:

```text
Phase 4: Large-Model and 90B Hybrid/Offload Validation
```

Suggested issues:

1. Define claim taxonomy and public evidence policy.
2. Add claim label config and validation.
3. Add hybrid plan data model.
4. Add `prism-plan-90b` simulated planner.
5. Extend manifest schema for 90B evidence.
6. Add usability threshold policy.
7. Add repeatability policy and evidence fields.
8. Add `prism-validate-claim` tool.
9. Add measured-offload evidence workflow or manual runbook.
10. Update `90b-hybrid-offline.json` to the final taxonomy.
11. Create Phase 4 evidence and exit audit.

## Test Gates

Hosted CI:

- Claim taxonomy unit tests.
- Hybrid planner unit tests.
- Usability policy tests.
- Config parser tests for claim labels.
- Manifest writer tests for 90B fields.
- `prism-plan-90b` simulated output tests.

Self-hosted/manual evidence:

- Measured 90B runs only with external artifacts.
- Repeatability runs.
- Cold-cache and warm-cache runs.
- Failure artifacts for rejected claims.

## Claim Validation Rules

Reject deployable claims when:

- claim label is missing,
- model hash is missing,
- quant artifact hash is missing,
- hardware/driver mode is missing,
- context length is missing,
- host memory peak is missing,
- GPU cap evidence is missing,
- quality gate is missing or failed,
- profitability/usability threshold is missing or failed,
- repeatability requirement is missing or failed,
- evidence is simulated or extrapolated only.

## Implementation Order

1. Claim taxonomy doc and config enum.
2. Hybrid plan data model.
3. 16 GiB maximum-cap and impossible-math gates.
4. Manifest v0.5 claim fields.
5. `prism-plan-90b` simulated planner.
6. Claim validation tool.
7. Usability policy.
8. Repeatability policy.
9. Update 90B config to final taxonomy.
10. Manual/self-hosted evidence runbook.
11. Phase 4 evidence document and exit audit.

## Exit Artifacts

Required artifacts for Phase 4 completion:

- `docs/claim-taxonomy.md`
- `docs/phase4-90b-validation-policy.md`
- `docs/phase4-evidence.md`
- validated evidence bundle schema fixture,
- rejected overclaim fixture,
- measured-offload or documented rejection artifact,
- deployment runbook only if `deployable-profile` is actually achieved.

## Exit Criteria

Phase 4 exits only when:

- 90B claims are impossible to publish without a claim label.
- Any `>16 GiB` GPU-cap claim is out of current scope.
- Simulated results cannot be marked deployable.
- Resident-memory claims fail unless the impossible-math gate passes.
- Measured-offload runs record exact model, quant, hardware, memory, IO,
  latency, quality, and repeatability evidence.
- Technically runnable but unusably slow runs are rejected.
- Incomplete artifact hashes are rejected as non-reproducible.
- Validated-benchmark and deployable-profile labels require retained artifacts.
