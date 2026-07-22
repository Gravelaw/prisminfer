# PrismInfer Cap Semantics

Phase 0 benchmark hard-cap mode certifies a run only when all required telemetry is present and the cap contract passes.

Default hard cap:

```text
1073741824 bytes
```

Current maximum configurable GPU hard cap:

```text
17179869184 bytes
```

This is 16 GiB. Configs, manifests, schemas, and claims must not promote a
GPU cap above this ceiling until a deliberate cap-policy change is made.

For resident GPU admission, later phases must account for:

```text
peak_vram =
  resident_weight_bytes
+ active_request_state_bytes
+ representation_metadata_bytes
+ admitted_workspace_peak_bytes
+ runtime_context_bytes
+ scheduler_queue_peak_bytes
+ batching_chunking_pool_peak_bytes
+ retained_shared_prefix_kv_cache_bytes
+ shared_cache_metadata_index_bytes
+ cache_eviction_workspace_peak_bytes
+ allocator_fragmentation_bytes
+ instrumentation_bytes
+ unknown_gpu_bytes
+ telemetry_safety_margin_bytes
```

These categories are mutually exclusive: active per-request state and retained
shared prefix/KV cache have different owners, and no allocation may be charged
twice or hidden in an undifferentiated pool. Certification is allowed only when
`peak_vram <= effective_live_cap_bytes <= hard_cap_bytes`, unknown promoted
bytes are zero, and all required evidence sources for the claim label are
present. The complete ledger contract is owned by
[`validation-matrix.md`](validation-matrix.md#memory-envelope).

Certification sources, in priority order:

1. PrismInfer allocator tracker.
2. CUDA runtime or driver process-visible allocation state.
3. NVML or platform process GPU telemetry where available.
4. Device-level memory delta as corroboration only.

Benchmark mode fails closed when telemetry is missing, contradictory, or incomplete.

Interactive mode may warn or fall back, but it must not claim hard-cap certification unless benchmark hard-cap mode passes.

## Current Phase 0 Behavior

`prism-probe` writes both JSONL telemetry and a minimal benchmark manifest. Results without a manifest are exploratory only.

Default outputs:

```text
prisminfer-probe.jsonl
prisminfer-manifest.json
```

The non-CUDA build remains valid. In that build, `1gb-safe-gpu-probed` fails closed with:

```text
cuda_probe_not_compiled
```

When `PRISMINFER_ENABLE_CUDA_PROBE=ON`, the probe links the CUDA runtime, creates a context with `cudaFree(nullptr)`, samples memory with CUDA memory APIs before and after context creation, and records driver/runtime version and GPU name when available.

## PrismInfer Allocator Tracker

The current PrismInfer allocator tracker is an owned-allocation accounting layer, not a full GPU allocator replacement.

It enforces:

```text
owned_current_bytes + requested_bytes <= hard_cap_bytes
```

It records:

```text
allocator_bytes
allocator_peak_bytes
rejected attempted peak
```

Rejected allocation attempts do not change current ownership, but they do raise the observed allocator peak so cap certification fails with:

```text
allocator_peak_exceeded_cap
```

This satisfies the Phase 0 requirement for allocator ownership accounting. A later Phase 1/llama.cpp bridge must connect actual backend allocations to this tracker before claiming full runtime allocation governance.

## Telemetry Agreement Policy

Phase 0 treats allocator/process/device sources as separate evidence streams.
The allocator tracker remains authoritative for PrismInfer-owned bytes, process
GPU telemetry is authoritative for process-visible GPU peaks when available,
and device-level deltas are corroborating evidence only.

Certification fails closed when:

```text
required_telemetry_missing
telemetry_disagreement
allocator_process_peak_disagreement
device_delta_process_disagreement
unified_memory_enabled
```

Default tolerances:

```text
allocator_process_tolerance_bytes = 16777216
device_delta_tolerance_bytes = 67108864
```

Device delta disagreement is evaluated only when a process GPU peak is present.
This avoids treating unrelated device noise as hard-cap proof while still
rejecting contradictory process/device evidence.

## Forced Breach Simulation

Use simulation flags to test fail-closed behavior without allocating real excessive GPU memory:

```text
--simulate-allocator-peak-bytes BYTES
--simulate-process-gpu-peak-bytes BYTES
--simulate-warmup-peak-bytes BYTES
--simulate-unknown-post-warmup-bytes BYTES
```

Expected failure reasons:

```text
allocator_peak_exceeded_cap
process_gpu_peak_exceeded_cap
warmup_peak_exceeded_cap
unknown_post_warmup_allocation
```

`scripts\verify.ps1` runs the full forced breach matrix and validates each
emitted telemetry file with `prism-validate-lifecycle`.
