# PrismInfer Cap Semantics

Phase 0 benchmark hard-cap mode certifies a run only when all required telemetry is present and the cap contract passes.

Default hard cap:

```text
1073741824 bytes
```

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
