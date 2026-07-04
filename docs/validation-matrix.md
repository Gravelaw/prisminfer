# Validation Matrix

This document is the canonical validation envelope for PrismInfer model-size and
VRAM-tier claims.

## Current Envelope

Current maximum GPU hard cap:

```text
16 GiB = 17179869184 bytes
```

No roadmap item, config, benchmark claim, or GitHub issue should require or
validate a GPU cap above 16 GiB until the cap policy is explicitly changed.

VRAM tiers:

```text
1 GiB, 2 GiB, 4 GiB, 6 GiB, 8 GiB, 12 GiB, 16 GiB
```

Model parameter buckets:

```text
<=2B
>2B-5B
>5B-10B
>10B-15B
>15B-20B
...
>85B-90B
```

## Cap Admission Equation

Each validation cell must account for the resident GPU memory envelope:

```text
peak_vram =
  resident_weight_bytes
+ resident_kv_bytes
+ activation_workspace_bytes
+ cuda_context_runtime_bytes
+ allocator_fragmentation_bytes
+ telemetry_safety_margin_bytes

certify only if peak_vram <= hard_vram_cap_bytes
```

Device-level memory delta is corroborating evidence only. Certification requires
allocator/process/backend evidence appropriate to the claim label.

## Cell Identity

Every benchmark or claim should identify a validation cell:

```text
validation_cell_id =
  model_parameter_bucket
  + model_hash
  + quantization_format
  + context_tokens
  + vram_tier_gib
  + backend
  + os
  + hardware_class
```

Required manifest/config fields:

```text
model_parameter_bucket
parameter_count
model_hash
quantization_format
context_tokens
vram_tier_gib
hard_vram_cap_bytes
validation_cell_id
validation_cell_status
```

## Cell Status

Allowed statuses:

| Status | Meaning |
|---|---|
| `not-started` | No retained evidence. |
| `metadata-only` | Model/config/sidecar checks only. |
| `warmup` | Backend warmup evidence exists; no decode claim. |
| `decode-smoke` | At least one bounded decode run exists. |
| `quality-gated` | Quality gates pass for this exact cell. |
| `profitable` | Transfer-inclusive comparison beats baseline for this cell. |
| `validated` | Cap, quality, performance, repeatability, and manifest gates pass. |
| `rejected` | A declared gate failed. |

## Advancement Rule

Do not promote a larger model bucket or more aggressive claim by analogy. A pass
in one cell does not imply a pass in another cell.

Examples:

- A pass in `>2B-5B @ 4 GiB` does not imply `>5B-10B @ 4 GiB`.
- A pass on one model family does not imply bucket-level generality.
- A 90B simulated plan does not imply measured or deployable inference.

Phase 1 should prove representative `<=2B` and `>2B-5B` backend cells before
larger buckets are claimed. Later phases expand the matrix through KV quality,
offload profitability, and large-model claim validation.

## Required Evidence Per Promoted Cell

Promoted cells require retained artifacts:

- telemetry JSONL,
- benchmark manifest,
- lifecycle validator result,
- dependency pin record,
- model hash and quantization artifact hash,
- backend and OS/hardware profile,
- cap certification or precise fail-closed reason,
- quality gate result when the claim mentions useful output,
- host memory and IO telemetry when CPU/offload participates,
- transfer metrics when GPU/offload profitability is claimed,
- repeatability evidence for validated or deployable labels.

## Initial Representative Ladder

| Bucket | Candidate status target | Notes |
|---|---|---|
| `<=2B` | `warmup`, then `decode-smoke` | First real backend and schema migration target. |
| `>2B-5B` | `warmup`, then `decode-smoke` | Required before larger claims. |
| `>5B-10B` | `quality-gated` | First practical constrained-inference quality lane. |
| `>10B-15B` | `quality-gated` or `rejected` | 12B/13B class, likely CPU-heavy under low caps. |
| `>20B-30B` | `measured-offload` or `rejected` | 26B class; offload and host pressure become central. |
| `>65B-70B` | `simulated` or `measured-offload` | Never promote without full evidence. |
| `>85B-90B` | `simulated`, `measured-offload`, or `rejected` | Terminal scale bucket under the current <=16 GiB envelope. |

