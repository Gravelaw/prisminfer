# Claim Taxonomy

PrismInfer claims must be scoped to a validation cell from
`docs/validation-matrix.md`. A claim without a validation cell is exploratory.

## Claim Labels

| Label | Meaning | Promotion requirement |
|---|---|---|
| `metadata-only` | Model/config/sidecar validation only. | Sidecar, model metadata, hash, and config checks pass. |
| `simulated` | Planner math, lower-bound estimate, or extrapolation only. | Plan is retained and impossible-memory math is explicit. |
| `warmup` | Backend warmup completed under a cap. | Lifecycle-valid warmup manifest and telemetry exist. |
| `decode-smoke` | A bounded decode run completed. | Warmup plus decode telemetry and cap result exist. |
| `quality-gated` | Output quality gates pass for the exact validation cell. | Baseline and candidate quality results are retained. |
| `profitable` | Transfer-inclusive performance beats baseline. | Same-cell baseline comparison passes profitability threshold. |
| `measured-offload` | Real offload execution exists but is not fully validated. | Host/IO/transfer evidence is retained. |
| `validated-benchmark` | Cap, quality, performance, repeatability, and evidence gates pass. | Evidence bundle validates. |
| `deployable-profile` | Validated benchmark plus operating constraints and runbook. | Deployment procedure is repeatable and bounded. |
| `rejected` | A declared gate failed. | Rejection reason and artifacts are retained. |

## Non-Promotion Rules

- `simulated` never promotes to `deployable-profile`.
- `metadata-only` never implies runnable inference.
- `decode-smoke` never implies useful output quality.
- `quality-gated` never implies transfer profitability.
- A pass in one validation cell never promotes another validation cell.
- A GPU cap above 16 GiB is outside the current roadmap.

## Required Scope

Every non-exploratory claim must record:

- validation cell id,
- model hash,
- model parameter bucket,
- quantization format,
- context length,
- backend,
- OS and hardware class,
- VRAM tier and hard cap,
- claim label,
- retained evidence paths.

