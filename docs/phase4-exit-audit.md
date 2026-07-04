# Phase 4 Exit Audit

| Exit criterion | Status | Evidence |
|---|---|---|
| 90B claims are impossible to publish without a claim label. | Pass | Claim label config, schemas, and `prism-validate-claim` require an explicit label. |
| Any `>16 GiB` GPU-cap claim is out of current scope. | Pass | `validate_hybrid_plan` rejects `hard_vram_cap_out_of_scope`. |
| Simulated results cannot be marked deployable. | Pass | Claim taxonomy and overclaim fixture reject simulated deployable evidence. |
| Resident-memory claims fail unless the impossible-math gate passes. | Pass | `test_hybrid_plan` covers resident budget rejection. |
| Measured-offload runs record exact model, quant, hardware, memory, IO, latency, quality, and repeatability evidence. | Pass | `validated-benchmark` and `deployable-profile` require retained evidence fields. |
| Technically runnable but unusably slow runs are rejected. | Pass | `test_usability_policy` rejects low decode throughput. |
| Incomplete artifact hashes are rejected as non-reproducible. | Pass | Claim taxonomy rejects missing model or quant evidence for measured/validated labels. |
| Validated-benchmark and deployable-profile labels require retained artifacts. | Pass | Evidence bundle validator rejects missing manifests, telemetry, quality, profitability, and repeatability. |

## Residual Boundaries

No deployable-profile claim is made. Real measured large-model runs remain
external-artifact and self-hosted/manual evidence work.

