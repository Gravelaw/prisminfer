# Phase 2 KV Baseline

Phase 2 baseline evidence uses deterministic fake-backend KV metadata before
promoting any compression or optimized kernel work. The baseline is a governed
accounting fixture, not a throughput or deployability claim.

## Baseline Cell

```powershell
.\build\Debug\prism-probe.exe `
  --backend fake `
  --model-parameter-bucket '<=2B' `
  --parameter-count 1000000 `
  --validation-cell-id phase2-kv-baseline-cell `
  --validation-cell-status decode-smoke `
  --context-tokens 64 `
  --warmup-tokens 2 `
  --kv-accounting on `
  --kv-layer-count 2 `
  --kv-head-count 4 `
  --kv-head-dim 8 `
  --kv-block-tokens 16 `
  --kv-placement host `
  --kv-compression accounting-only `
  --quality-gate smoke `
  --quality-baseline-score 1.0 `
  --quality-observed-score 1.0 `
  --quality-max-delta 0.0 `
  --telemetry phase2-kv-baseline.jsonl `
  --manifest phase2-kv-baseline-manifest.json `
  --run-id phase2-kv-baseline
.\build\Debug\prism-validate-lifecycle.exe phase2-kv-baseline.jsonl
```

Result:

```text
status: ok
lifecycle valid: true
kv_bytes_per_token: 256
kv_logical_tokens: 66
kv_host_bytes: 16896
kv_metadata_bytes: 320
kv_evidence_status: reconciled
quality_status: passed
```

## Pinned Backend Baseline

The pinned llama.cpp/GGUF backend baseline is retained as fail-closed evidence
because llama.cpp external allocation is still observed but not governed:

```text
status: failed_closed
failure_reason: unknown_post_warmup_allocation
lifecycle valid: true
backend: llama
kv_bytes_per_token: 6912
kv_logical_tokens: 65
kv_host_bytes: 449280
kv_metadata_bytes: 320
kv_evidence_status: estimated
quality_status: passed
```

This artifact proves Phase 2 KV lifecycle and manifest fields can be emitted for
a real pinned backend/model, but it does not certify llama.cpp allocation
governance.
