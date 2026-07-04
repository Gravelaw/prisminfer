# PrismInfer Risk Register

| Risk | Severity | Phase | Mitigation | Current status | Remaining blocker |
|---|---:|---|---|---|---|
| CUDA context consumes most of 1GB | High | 0 | probe before GPU use; fail closed | CUDA context probe and GPU smoke exist | Retain successful self-hosted CUDA artifacts for Phase 0 exit |
| Telemetry disagreement on Windows/WDDM | High | 0 | benchmark mode fails closed; allocator tracker, CUDA runtime, and NVML evidence are reported separately | Disagreement reasons and tolerances are explicit | Review real CUDA artifacts across Class B/C hardware |
| Owned allocation escapes PrismInfer accounting | High | 0/1 | Phase 0 adds capped allocator tracker; Phase 1 must connect llama.cpp/backend allocation paths before claiming full governance | Owned allocation tracker and breach tests exist | llama.cpp/backend allocation bridge remains Phase 1 |
| llama.cpp hidden allocations | High | 1 | mandatory backend warmup before certification | Phase 0 backend warmup placeholder event exists | Replace placeholder with real llama.cpp warmup in Phase 1 |
| GPU offload slower than CPU | High | 3 | transfer-inclusive profitability rule | Out of Phase 0 scope | Needs Phase 3 benchmark policy |
| KV compression passes perplexity but fails retrieval | High | 2 | task-level quality gates | Out of Phase 0 scope | Needs Phase 2 quality gates |
| 90B hybrid appears feasible but is unusably slow | High | 4 | simulated and validated labels remain separate | Out of Phase 0 scope | Needs Phase 4 validated evidence policy |

## Original Plan Traceability

| Original plan item | Current status | Evidence |
|---|---|---|
| `allocator_tracker.cpp` | Implemented Phase 0 accounting foundation | `CappedAllocatorTracker` tracks current, peak, and rejected attempted peak |
| Forced cap-breach test | Implemented for allocator/process/warmup/unknown paths | `--simulate-*` flags and lifecycle-valid telemetry |
| Cap certification fail-closed behavior | Implemented for current Phase 0 telemetry model | `certify_cap` rejects cap breach, disagreement, missing telemetry, unified memory, and unknown post-warmup allocation |
| Process/device memory reports | Partially implemented | CUDA runtime plus NVML evidence in CUDA-probed build |
| Backend warmup probe | Not yet implemented | blocked until llama.cpp/backend bridge work |
