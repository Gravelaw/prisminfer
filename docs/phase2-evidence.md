# Phase 2 Evidence

This file records Phase 2 KV accounting and quality-gate evidence. The
artifacts are generated locally and intentionally ignored by `.gitignore`; hash
values bind the retained local files to this evidence record.

## Implementation Baseline

Phase 2 currently provides:

- KV profile and ledger model,
- KV accounting CLI/config fields,
- telemetry and benchmark manifest schema v0.3,
- lifecycle v0.3 events for KV profile, prefill, decode, and quality results,
- deterministic quality gate evaluator and `prism-quality`,
- KV compression claim evaluator for `none`, `accounting-only`, `reference`,
  and `experimental`,
- fail-closed rejection for KV metadata-budget breach,
- fail-closed rejection for compression claims without quality evidence.

Phase 2 does not provide:

- optimized KV compression kernels,
- certified llama.cpp allocation governance,
- offload profitability claims,
- generalized quality claims across validation cells.

## Evidence Artifacts

Accounting-only baseline:

```text
artifact: phase2-kv-baseline.jsonl
manifest: phase2-kv-baseline-manifest.json
status: ok
kv_bytes_per_token: 256
kv_logical_tokens: 66
kv_host_bytes: 16896
kv_metadata_bytes: 320
kv_evidence_status: reconciled
quality_status: passed
```

Pinned llama.cpp/GGUF KV baseline:

```text
artifact: phase2-real-llama-15m-kv-baseline.jsonl
manifest: phase2-real-llama-15m-kv-baseline-manifest.json
status: failed_closed
failure_reason: unknown_post_warmup_allocation
backend_external_peak_bytes: 26413622
kv_bytes_per_token: 6912
kv_logical_tokens: 65
kv_host_bytes: 449280
kv_metadata_bytes: 320
kv_evidence_status: estimated
quality_status: passed
```

Reference compression rejection:

```text
artifact: phase2-reference-compression-rejected.jsonl
manifest: phase2-reference-compression-rejected-manifest.json
status: failed_closed
failure_reason: quality_gate_required_for_compression
kv_compression: reference
quality_status: skipped
kv_compression_accepted: false
```

KV metadata budget rejection:

```text
artifact: phase2-kv-budget-breach.jsonl
manifest: phase2-kv-budget-breach-manifest.json
status: failed_closed
failure_reason: kv_metadata_budget_exceeded
kv_metadata_bytes: 320
kv_metadata_budget_bytes: 1
```

## Artifact Hashes

| Artifact | SHA-256 |
|---|---|
| `phase2-kv-baseline.jsonl` | `15EAE81812CF675AA470E50BE17B366A1FE053E9B7C1E10FB70539216B0B9496` |
| `phase2-kv-baseline-manifest.json` | `3EDB74C828B46FDE85664DE96E350745B4F9D79D0F0C54345E4F2BE818D9133C` |
| `phase2-real-llama-15m-kv-baseline.jsonl` | `1731AED11DE555E544D72913F393C9FBFBF21BF7F1D9FFF93F7998CA9AEB5ECD` |
| `phase2-real-llama-15m-kv-baseline-manifest.json` | `C6F70E77367EC7471A39FA5CC767B828D58E4D575BFB854084B6FD6DE60BC48E` |
| `phase2-reference-compression-rejected.jsonl` | `22B356F4CC80BCA2D3E24B89CAFBF0C35F072A1C6E3BB943F2B35129D7AAD197` |
| `phase2-reference-compression-rejected-manifest.json` | `E445B2F775595312F1123BD7DCBA344D5A0912BD509E569FD6EA3180F885F856` |
| `phase2-kv-budget-breach.jsonl` | `5044752046DA280972F379DF94662B35847313ABE8FF355BE87EB5F25653849F` |
| `phase2-kv-budget-breach-manifest.json` | `32FC947EEE0DFD7822A874760D3ED638F393A6C4853C6C0CBB8056D3BE26CDDD` |

## Claim Status

Allowed current Phase 2 claims:

- PrismInfer reports KV profile and tier bytes from explicit metadata.
- PrismInfer emits lifecycle-valid KV profile, prefill, decode, and quality
  events.
- PrismInfer rejects compression claims without quality evidence.
- PrismInfer rejects KV metadata-budget breach.
- PrismInfer can record a pinned llama.cpp/GGUF KV baseline as fail-closed
  evidence.

Disallowed current Phase 2 claims:

- PrismInfer has optimized KV compression kernels.
- PrismInfer has certified llama.cpp allocation governance.
- PrismInfer can claim compression quality from perplexity or score-only
  evidence.
- PrismInfer can generalize quality or memory results across validation cells.
