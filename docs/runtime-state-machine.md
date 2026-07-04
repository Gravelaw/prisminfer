# PrismInfer Runtime State Machine

Phase 0 implements the initial subset of the final lifecycle:

```text
run_start
config_validated | failed_closed
telemetry_probe
model_sidecar_validated
cuda_context_probe, if GPU requested
cap_semantics_resolved
host_prepare
backend_warmup
cap_certification_result
completed | failed_closed
run_end
```

Phase 2 adds KV accounting, prefill/decode, and quality-gate events.

Every event includes a `run_id` so telemetry can be correlated with the manifest.

`backend_warmup` is a Phase 0 placeholder event. It records that no llama.cpp
backend is attached yet and exposes the warmup peak used by cap certification.
Phase 1 must replace the placeholder with real backend warmup evidence before
claiming full runtime allocation governance.

`model_sidecar_validated` is a Phase 0 metadata gate. It validates optional
`--model` and `--sidecar` inputs for normalized paths, file size limits,
sidecar schema shape, and model hash logging without loading llama.cpp.

Phase 0 lifecycle validation currently accepts either:

- a CPU-safe path with no `cuda_context_probe`,
- a GPU-probed path with `cuda_context_probe` followed by certification or fail-closed telemetry, or
- an early CUDA fail-closed path when the CUDA context cannot be created.

When `kv_cache_profile` appears, lifecycle validation requires this ordered
segment after `backend_warmup` and before `memory_sample`:

```text
kv_cache_profile
prefill_start
kv_cache_sample
prefill_end
decode_start
decode_token
decode_end
quality_gate_result
```

`quality_gate_result` must precede `memory_sample` so cap certification and
manifests cannot be emitted before the quality/compression decision is known.
