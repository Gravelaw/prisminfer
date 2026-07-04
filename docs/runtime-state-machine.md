# PrismInfer Runtime State Machine

Phase 0 implements the initial subset of the final lifecycle:

```text
run_start
config_validated | failed_closed
telemetry_probe
cuda_context_probe, if GPU requested
cap_semantics_resolved
host_prepare
cap_certification_result
completed | failed_closed
run_end
```

Later phases add model mmap, llama baseline planning, backend warmup, tokenizer validation, prefill, ready-for-first-token, and token events.

Every event includes a `run_id` so telemetry can be correlated with the manifest.

Phase 0 lifecycle validation currently accepts either:

- a CPU-safe path with no `cuda_context_probe`, or
- a GPU-probed path with `cuda_context_probe` followed by certification or fail-closed telemetry.
