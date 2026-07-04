# Phase 2 Exit Audit

Status: Pass with classified residual risk.

Phase 2 closes KV accounting and quality-gate scaffolding. It does not close
llama.cpp allocation governance or offload profitability.

## Exit Criteria

| Criterion | Status | Evidence |
|---|---|---|
| KV cache bytes are reported by profile and by tier. | Pass | Manifests record profile fields, host/GPU/compressed/metadata/unknown bytes. |
| Baseline uncompressed KV evidence exists for at least one pinned backend/model. | Pass | `phase2-real-llama-15m-kv-baseline.*` records pinned llama.cpp/GGUF metadata extracted from backend logs and fails closed on unreconciled external allocation. |
| Compression profiles cannot claim success without a quality gate. | Pass | `phase2-reference-compression-rejected.*` fails closed with `quality_gate_required_for_compression`. |
| Perplexity-only success is explicitly insufficient. | Pass | `docs/phase2-quality-gates.md` requires smoke/retrieval/long-context evidence scoped to a validation cell. |
| Compression and KV quality results are scoped to an exact validation cell. | Pass | Manifest v0.3 binds KV, quality, backend, model bucket, and validation-cell fields. |
| Manifest and telemetry agree on KV profile, compression policy, quality status, and cap result. | Pass | Lifecycle validator accepts the v0.3 evidence JSONL files; manifests retain matching status/failure reasons. |
| Hosted CI covers deterministic ledger/quality logic. | Pass | Unit tests cover KV ledger and quality/compression evaluators. |
| Self-hosted CUDA evidence remains lifecycle-valid when KV accounting is on. | Classified | CUDA workflow remains available; Application Control blocks are classified. Phase 2 local evidence is CPU/fake plus pinned llama CPU. |

## Residual Risk

KV bytes from explicit metadata are deterministic, but real llama.cpp allocation
governance remains unresolved. Any Phase 3 offload or profitability claim must
use transfer-inclusive evidence and must not promote Phase 2 estimated KV
evidence into certified backend allocation governance.
