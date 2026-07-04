# Phase 2 Implementation Plan: KV Cache Accounting and Compression Gates

Phase 2 starts only after Phase 1 can produce real backend warmup evidence for a
pinned llama.cpp/GGML/GGUF backend. Its purpose is to make KV cache memory
observable, then evaluate compression and placement strategies through explicit
quality gates.

## Phase 2 Goal

Account for KV cache growth by model, layer, head, token, context length, and
placement; then test KV compression strategies only when memory savings and
task-level quality evidence are both recorded.

## Non-Goals

- No deployable 90B claim.
- No custom CUDA KV kernel until backend governance and quality harnesses pass.
- No compression success claim from perplexity alone.
- No prefix/KV reuse without trust-boundary and cache-isolation metadata.
- No offload profitability claim; that is Phase 3.

## Architecture Direction

Phase 2 should introduce a KV ledger before any compression kernel or custom
attention implementation. The ledger must work even when llama.cpp owns the
physical KV allocation.

Hard-cap certification requires KV evidence to be classified:

| Status | Meaning |
|---|---|
| `estimated` | Derived from metadata or formula only. |
| `measured` | Observed through backend/process/device telemetry. |
| `reconciled` | Estimated and measured evidence agree within tolerance. |

Only `reconciled` KV evidence can support a certified backend run. Estimated KV
evidence remains useful for planning and rejection, but not for promoted
certification.

Proposed abstractions:

```cpp
struct KvCacheProfile {
  std::uint32_t layer_count;
  std::uint32_t kv_head_count;
  std::uint32_t head_dim;
  std::uint32_t block_tokens;
  std::uint64_t bytes_per_token;
  std::uint64_t bytes_per_block;
};

struct KvCacheSample {
  std::uint64_t logical_tokens;
  std::uint64_t active_blocks;
  std::uint64_t reusable_blocks;
  std::uint64_t evicted_blocks;
  std::uint64_t gpu_bytes;
  std::uint64_t host_bytes;
  std::uint64_t compressed_bytes;
  std::uint64_t metadata_bytes;
};

class KvCacheLedger {
 public:
  void record_profile(const KvCacheProfile& profile);
  void sample(const KvCacheSample& sample);
};
```

Initial policies:

- `none`: uncompressed baseline.
- `accounting-only`: estimates KV bytes from backend metadata and observed peaks.
- `reference-kv-quant`: CPU/reference path for research validation only.
- `experimental`: reserved for KIVI/KVQuant or PolarQuant/TurboQuant lanes after
  quality harness exists.

## Lifecycle v0.3 Target

Phase 2 adds KV and token-stage events:

```text
kv_cache_profile
prefill_start
prefill_end
kv_cache_sample
decode_start
decode_token
decode_end
quality_gate_result
```

Target sequence for a compressed or KV-instrumented run:

```text
run_start
config_validated
telemetry_probe
model_sidecar_validated
backend_selected
dependency_pins_resolved
cuda_context_probe, if GPU requested
cap_semantics_resolved
host_prepare
model_load_plan
backend_init
backend_warmup
kv_cache_profile
prefill_start
kv_cache_sample
prefill_end
decode_start
decode_token, repeated
decode_end
quality_gate_result
memory_sample
cap_certification_result
completed | failed_closed
run_end
```

## Config and CLI Additions

Add config fields:

```json
{
  "kv_accounting": true,
  "kv_block_tokens": 16,
  "kv_placement": "backend|gpu|host|mixed",
  "kv_compression": "none|reference|experimental",
  "kv_key_bits": 16,
  "kv_value_bits": 16,
  "kv_metadata_budget_bytes": 0,
  "quality_gate": "none|smoke|retrieval|long-context",
  "quality_baseline_manifest": ""
}
```

Add CLI flags:

```text
--kv-accounting on|off
--kv-block-tokens N
--kv-placement backend|gpu|host|mixed
--kv-compression none|reference|experimental
--kv-key-bits N
--kv-value-bits N
--kv-metadata-budget-bytes N
--quality-gate none|smoke|retrieval|long-context
--quality-baseline-manifest PATH
```

## Manifest v0.3 Additions

Record:

- KV profile fields: layers, KV heads, head dimension, block size.
- KV placement policy.
- KV payload bytes.
- KV metadata bytes.
- KV peak bytes by tier.
- KV compression algorithm family.
- Effective bits per value.
- Quality gate id and version.
- Baseline manifest path/hash.
- Quality pass/fail status.
- Rejection threshold and observed delta.

## File and Module Candidates

New files:

```text
include/prisminfer/kv_cache_ledger.h
include/prisminfer/kv_compression_policy.h
include/prisminfer/quality_gate.h
src/kv/kv_cache_ledger.cpp
src/kv/kv_compression_policy.cpp
src/quality/quality_gate.cpp
tools/prism-quality/main.cpp
tests/unit/test_kv_cache_ledger.cpp
tests/unit/test_quality_gate.cpp
docs/phase2-kv-baseline.md
docs/phase2-evidence.md
```

Likely modified files:

```text
CMakeLists.txt
include/prisminfer/config.h
include/prisminfer/runtime_state.h
include/prisminfer/benchmark.h
include/prisminfer/telemetry.h
src/config.cpp
src/benchmark/manifest_writer.cpp
src/telemetry/telemetry_jsonl.cpp
tools/prism-probe/main.cpp
tools/prism-validate-lifecycle/main.cpp
schemas/telemetry.schema.json
schemas/benchmark_manifest.schema.json
docs/runtime-state-machine.md
docs/kv-cache-and-compression-research.md
docs/risk-register.md
```

## Detailed Implementation Slices

### Planner Agent Slice Map

| Slice | GitHub issue candidate | File/module candidates | Gate |
|---|---|---|---|
| P2-01 | Add KV ledger model and event constants | `include/prisminfer/kv_cache_ledger.h`, `src/kv/kv_cache_ledger.cpp`, `include/prisminfer/runtime_state.h` | Ledger records layer/head/token/block/tier bytes without compression. |
| P2-02 | Extract KV profile from backend metadata | `include/prisminfer/backend.h`, `src/backend/llama_backend.cpp`, `src/validation/model_sidecar.cpp` | GGUF/backend metadata is used; no family-name inference. |
| P2-03 | Extend telemetry/manifest to lifecycle v0.3 | `schemas/telemetry.schema.json`, `schemas/benchmark_manifest.schema.json`, `src/benchmark/manifest_writer.cpp`, `tools/prism-validate-lifecycle/main.cpp` | `kv_cache_profile`, `prefill_*`, `decode_*`, and `quality_gate_result` validate in order. |
| P2-04 | Add uncompressed KV baseline mode | `tools/prism-probe/main.cpp`, `configs/kv-baseline-*.json`, `docs/phase2-kv-baseline.md` | Baseline manifest exists before any compression comparison. |
| P2-05 | Add deterministic quality harness | `tools/prism-quality/main.cpp`, `src/quality/quality_gate.cpp`, `tests/unit/test_quality_gate.cpp` | Prompt fixtures cover token divergence, retrieval, long-context smoke, TTFT, and decode rate. |
| P2-06 | Add compression policy interface | `include/prisminfer/kv_compression_policy.h`, `src/kv/kv_compression_policy.cpp` | `none`, `accounting-only`, and `reference` policies are testable without custom kernels. |
| P2-07 | Add research experiment lanes | `src/kv/policies/`, `docs/kv-cache-and-compression-research.md` | KIVI/KVQuant/PolarQuant/TurboQuant remain experimental until quality passes. |
| P2-08 | Phase 2 evidence and audit | `docs/phase2-evidence.md`, `docs/risk-register.md` | Memory savings, metadata overhead, quality result, and cap result agree across telemetry and manifest. |

### P2-A: KV Metadata Model

Purpose: make KV size and placement calculations deterministic before touching
backend memory.

Add:

```text
include/prisminfer/kv_model_metadata.h
src/kv/kv_model_metadata.cpp
tests/unit/test_kv_model_metadata.cpp
```

Responsibilities:

- derive KV bytes/token from layer count, KV heads, head dimension, datatype,
  block size, and context length,
- reject incomplete model metadata in benchmark mode,
- distinguish logical KV payload bytes from metadata/accounting bytes,
- expose stable JSON fields for manifests and telemetry.

Acceptance:

- handles GQA/MQA/full-attention shapes,
- detects overflow and invalid zero fields,
- emits the same byte count from config, telemetry, and manifest code paths.

### P2-B: KV Ledger and Budget Enforcement

Purpose: account for KV ownership even when llama.cpp owns the underlying
allocation.

Add or extend:

```text
include/prisminfer/kv_cache_ledger.h
src/kv/kv_cache_ledger.cpp
tests/unit/test_kv_cache_ledger.cpp
```

Responsibilities:

- track logical tokens, active blocks, reusable blocks, evicted blocks,
  compressed bytes, metadata bytes, GPU bytes, host bytes, and unknown bytes,
- mark estimates separately from measured samples,
- fail closed when KV metadata budget or KV owned bytes exceed declared caps,
- retain enough history for prefill and decode samples.

Acceptance:

- forced KV budget breach produces a lifecycle-valid `failed_closed` event,
- unknown KV ownership cannot be certified as a hard-cap run,
- ledger samples survive manifest round-trip tests.

### P2-C: Compression Claim Evaluator

Purpose: keep compression research claims honest before optimized kernels exist.

Add:

```text
include/prisminfer/kv_compression_policy.h
src/kv/policies/no_compression.cpp
src/kv/policies/reference_quantized_kv.cpp
src/quality/compression_claim_evaluator.cpp
tests/unit/test_compression_claim_evaluator.cpp
tools/prism-kv-report/main.cpp
docs/phase2-quality-gates.md
```

Responsibilities:

- compare uncompressed and compressed manifests,
- require both memory improvement and task-level quality acceptance,
- require matching validation cell fields: model hash, quantization, context,
  VRAM tier, backend, OS, and hardware class,
- label KIVI/KVQuant/PolarQuant/TurboQuant/QJL experiments as metadata-only
  until actual implementation and quality evidence exist,
- reject perplexity-only pass as insufficient.

Acceptance:

- `prism-kv-report` returns pass/fail/rejected with reasons,
- quality threshold boundaries are unit-tested,
- metadata-only experiments cannot be promoted to validated compression.

## Issue Slices

Create a Phase 2 milestone:

```text
Phase 2: KV Cache Accounting and Compression Gates
```

Suggested issues:

1. Define KV cache ledger data model.
2. Add KV accounting config and CLI contract.
3. Extend telemetry lifecycle to v0.3 for KV and token events.
4. Extend benchmark manifest with KV profile and quality fields.
5. Add baseline uncompressed KV measurement mode.
6. Add quality gate harness with deterministic prompt fixtures.
7. Add retrieval and long-context quality gate definitions.
8. Add reference KV compression policy interface.
9. Add KIVI/KVQuant metadata-only experiment lane.
10. Add PolarQuant/TurboQuant/QJL metadata-only experiment lane.
11. Create Phase 2 evidence and exit audit.

## Test Gates

Hosted CI:

- KV ledger unit tests.
- Config parser tests for KV fields.
- Manifest writer tests for KV and quality fields.
- Lifecycle validator tests for KV event order.
- Quality gate smoke tests with tiny deterministic fixtures.

Self-hosted CUDA CI:

- GPU-probed run with KV accounting enabled.
- Forced breach tests where KV metadata or KV bytes exceed declared budget.
- Artifact upload for KV telemetry and manifests.

Local optional evidence:

- Real backend run with uncompressed KV baseline.
- Real backend run with reference compression only after baseline exists.

## Quality Gates

Minimum gates:

- baseline token divergence at temperature 0,
- perplexity delta where supported,
- deterministic prompt regression,
- needle-style retrieval,
- long-context retrieval or summarization,
- time-to-first-token,
- prefill tokens/sec,
- decode tokens/sec,
- certified peak memory.

Reject if:

- retrieval fails even when perplexity passes,
- metadata overhead erases the memory savings,
- decode overhead makes compressed mode slower without a declared reason,
- llama.cpp-owned KV allocation is only partially observable,
- quality or compression evidence is generalized across validation cells,
- cap certification relies only on device delta.

## Implementation Order

1. KV ledger interface and tests.
2. KV config and CLI additions.
3. Telemetry and manifest v0.3 additions.
4. Lifecycle validator v0.3 support.
5. Uncompressed KV baseline mode.
6. Quality gate harness.
7. Reference compression policy interface.
8. KIVI/KVQuant metadata lane.
9. PolarQuant/TurboQuant/QJL metadata lane.
10. Phase 2 evidence document and exit audit.

## Exit Artifacts

Required artifacts before opening Phase 3:

- `docs/phase2-kv-baseline.md`
- `docs/phase2-quality-gates.md`
- `docs/phase2-evidence.md`
- uncompressed baseline manifest and telemetry fixture,
- at least one rejected compression claim fixture,
- at least one accepted accounting-only run fixture.

## Exit Criteria

Phase 2 exits only when:

- KV cache bytes are reported by profile and by tier.
- Baseline uncompressed KV evidence exists for at least one pinned backend/model.
- Compression profiles cannot claim success without a quality gate.
- Perplexity-only success is explicitly insufficient.
- Compression and KV quality results are scoped to an exact validation cell.
- Manifest and telemetry agree on KV profile, compression policy, quality status,
  and cap result.
- Hosted CI covers deterministic ledger/quality logic.
- Self-hosted CUDA evidence remains lifecycle-valid when KV accounting is on.
