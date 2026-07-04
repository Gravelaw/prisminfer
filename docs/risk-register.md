# PrismInfer Risk Register

| Risk | Severity | Phase | Mitigation |
|---|---:|---|---|
| CUDA context consumes most of 1GB | High | 0 | probe before GPU use; fail closed |
| Telemetry disagreement on Windows/WDDM | High | 0 | benchmark mode fails closed; allocator tracker, CUDA runtime, and NVML evidence are reported separately |
| Owned allocation escapes PrismInfer accounting | High | 0/1 | Phase 0 adds capped allocator tracker; Phase 1 must connect llama.cpp/backend allocation paths before claiming full governance |
| llama.cpp hidden allocations | High | 1 | mandatory backend warmup before certification |
| GPU offload slower than CPU | High | 3 | transfer-inclusive profitability rule |
| KV compression passes perplexity but fails retrieval | High | 2 | task-level quality gates |
| 90B hybrid appears feasible but is unusably slow | High | 4 | simulated and validated labels remain separate |

## Original Plan Traceability

| Original plan item | Current status | Evidence |
|---|---|---|
| `allocator_tracker.cpp` | Implemented Phase 0 accounting foundation | `CappedAllocatorTracker` tracks current, peak, and rejected attempted peak |
| Forced cap-breach test | Implemented for allocator/process/warmup/unknown paths | `--simulate-*` flags and lifecycle-valid telemetry |
| Cap certification fail-closed behavior | Implemented for current Phase 0 telemetry model | `certify_cap` rejects cap breach, disagreement, missing telemetry, unified memory, and unknown post-warmup allocation |
| Process/device memory reports | Partially implemented | CUDA runtime plus NVML evidence in CUDA-probed build |
| Backend warmup probe | Not yet implemented | blocked until llama.cpp/backend bridge work |
