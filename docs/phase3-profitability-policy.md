# Phase 3 Profitability Policy

Phase 3 treats GPU, host KV, and NVMe offload as useful only when the full
candidate path beats the CPU baseline within the same validation cell.

## Required Inputs

- same validation cell id for baseline and candidate,
- CPU baseline time-to-first-token and decode tokens/sec,
- candidate time-to-first-token and decode tokens/sec,
- H2D/D2H bytes for GPU or host offload,
- NVMe bytes and cold-cache marker for NVMe policies,
- pinned host and staging budgets where host or NVMe pressure participates,
- cap and quality gate status before promotion.

## Policy Modes

| Mode | Behavior |
|---|---|
| `off` | Records no profitability claim. |
| `warn` | Records slower or incomplete candidates without promoting them. |
| `fail-closed` | Rejects missing, mismatched, or slower candidates. |

## Rejection Reasons

| Reason | Meaning |
|---|---|
| `validation_cell_mismatch` | Baseline and candidate do not describe the same cell. |
| `transfer_metrics_required` | Offload was requested without transfer evidence. |
| `transfer_bytes_required` | No H2D, D2H, or NVMe bytes were recorded. |
| `nvme_bytes_required` | NVMe policy had no NVMe byte evidence. |
| `cold_cache_required_for_nvme` | NVMe policy lacked cold-cache evidence. |
| `offload_not_profitable` | Candidate speedup was below the configured threshold. |

## Boundary

This policy does not claim a deployable large-model profile. It only permits a
`profitable` label for the exact retained validation cell.

