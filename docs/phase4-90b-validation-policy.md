# Phase 4 Large-Model and 90B Validation Policy

Phase 4 validates large-model claims under the current maximum GPU cap of
16 GiB. It is not a promise that 70B or 90B dense models fit fully in VRAM.

## Scope

The terminal bucket is `>85B-90B`, but Phase 4 should validate progressively
through intermediate buckets before accepting a 90B claim.

Allowed claim labels:

| Label | Meaning |
|---|---|
| `metadata-only` | Model/config/sidecar validation only. |
| `simulated` | Planner math, lower-bound estimate, or extrapolation only. |
| `measured-offload` | Real execution with bounded evidence, not yet validated. |
| `validated-benchmark` | Cap, quality, latency, memory, IO, and repeatability gates pass. |
| `deployable-profile` | Validated benchmark plus operating constraints and runbook. |
| `rejected` | A declared gate failed. |

## Impossible-Math Gate

Reject resident or deployable claims when:

```text
resident_weight_bytes
+ quantization_metadata_bytes
+ resident_kv_bytes
+ activation_workspace_bytes
+ runtime_overhead_bytes
+ safety_margin_bytes
> hard_vram_cap_bytes
```

For the current roadmap, `hard_vram_cap_bytes` must be less than or equal to
`17179869184`.

## Evidence Requirements

Large-model validated claims require:

- model id and model hash,
- parameter count and parameter bucket,
- quantization format and artifact hash,
- shard list and hashes when sharded,
- exact context length and batch size,
- validation cell id,
- GPU cap and GPU telemetry,
- host memory and IO telemetry,
- transfer metrics for offload,
- quality gate result,
- usability thresholds,
- repeatability result,
- retained manifest and telemetry artifacts.

## Usability Gate

A run that emits a token eventually is not automatically useful.

Reject or label as non-deployable when:

- time-to-first-token exceeds the configured maximum,
- sustained decode tokens/sec is below the configured minimum,
- p95 token latency exceeds the configured maximum,
- repeated runs vary beyond the configured tolerance,
- host/NVMe pressure is unbounded or unmeasured.

