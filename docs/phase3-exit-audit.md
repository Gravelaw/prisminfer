# Phase 3 Exit Audit

| Exit criterion | Status | Evidence |
|---|---|---|
| CPU-only and GPU/offload profiles are comparable through manifests. | Pass | `phase3-profitability-accepted-manifest.json` and `phase3-profitability-rejected-manifest.json` retain same-cell baseline and candidate metrics. |
| Transfer metrics are included in the profitability decision. | Pass | `transfer_sample` precedes `profitability_result`; manifests retain H2D/D2H/NVMe fields. |
| Comparisons are rejected when validation cells differ. | Pass | `test_profitability_policy` covers mismatch rejection. |
| Slower GPU/offload runs are rejected or clearly labeled. | Pass | `phase3-profitability-rejected.*` fails closed with `offload_not_profitable`. |
| Pinned host and staging buffers are budgeted and reported. | Pass | `test_offload_planner` covers required budgets. |
| Cold-cache and warm-cache distinctions are explicit for NVMe paths. | Pass | `phase3-nvme-cold-cache-rejected.*` fails closed with `cold_cache_required_for_nvme`. |
| Self-hosted CUDA evidence includes accepted and rejected profitability cases. | Pass | `.github/workflows/offload-profitability-self-hosted.yml` produces accepted and rejected artifacts on the Windows NVIDIA runner. |

## Residual Boundaries

Phase 3 does not certify optimized offload scheduling, GPUDirect Storage,
unified-memory oversubscription, deployable 90B inference, or profitability
outside the exact retained validation cell.
