# PrismInfer Risk Register

| Risk | Severity | Phase | Mitigation | Current status | Remaining blocker |
|---|---:|---|---|---|---|
| CUDA context consumes most of 1GB | High | 0 | probe before GPU use; fail closed | CUDA context probe and GPU smoke exist | Retain successful self-hosted CUDA artifacts for Phase 0 exit |
| Telemetry disagreement on Windows/WDDM | High | 0 | benchmark mode fails closed; allocator tracker, CUDA runtime, and NVML evidence are reported separately | Disagreement reasons and tolerances are explicit | Review real CUDA artifacts across Class B/C hardware |
| Owned allocation escapes PrismInfer accounting | High | 0/1 | Phase 0 adds capped allocator tracker; Phase 1 adds backend adapter, fake backend, observer, and ledger contracts before real llama.cpp claims | Owned allocation tracker, backend observer, memory ledger, and fake backend breach tests exist | llama.cpp allocation bridge remains required before full governance claims |
| llama.cpp hidden allocations | High | 1 | mandatory backend warmup before certification | process-backed llama.cpp warmup can load pinned `<=2B` and `>2B-5B` GGUF artifacts and records external allocation evidence; certification fails closed on unreconciled external bytes | Reconcile llama.cpp allocation evidence with governed/device memory before certification claims |
| GPU offload slower than CPU | High | 3 | transfer-inclusive profitability rule | Out of Phase 0 scope | Needs Phase 3 benchmark policy |
| KV compression passes perplexity but fails retrieval | High | 2 | task-level quality gates | Out of Phase 0 scope | Needs Phase 2 quality gates |
| Large-model or 90B hybrid appears feasible but is unusably slow | High | 4 | simulated and validated labels remain separate | Out of Phase 0 scope | Needs Phase 4 validated evidence policy |
| Large-model or 90B under <=16 GiB overclassified as deployable | Critical | 4 | enforce claim taxonomy: metadata-only, simulated, measured-offload, validated-benchmark, deployable-profile | Current 90B config is labeled simulated and not deployable | Need public evidence policy before any large-model claim |
| Empty or no-model telemetry treated as inference proof | High | 0/1 | distinguish probe harness certification from real backend certification | lifecycle v0.2 records backend selection, dependency pin resolution, model load plan, backend init, and backend warmup; real `<=2B` and `>2B-5B` llama.cpp rejection artifacts exist | Allocation reconciliation still required before certified real-backend governance claims |
| CPU/NVMe offload shifts cap pressure outside VRAM | High | 2/4 | track RSS, commit, mmap residency proxy, pinned host memory, page faults, IO bytes, and pagefile pressure where available | Phase 1 records Windows process working set/private bytes, commit, available memory, and process IO in manifests | Pinned host memory, page faults, mmap residency proxy, cold-cache protocol, and NVMe bytes remain Phase 3/4 work |
| WDDM TDR or driver mode invalidates long-running paths | High | 2/3 | record driver mode, TDR assumptions, and validate long kernels/offload on appropriate driver modes | Not instrumented | Needs driver-mode and watchdog policy |
| PCIe/NVMe roofline invalidates profitability | High | 3/4 | require H2D/D2H bytes per token, NVMe bytes per token, p50/p95 tokens/sec, and CPU-only comparison | Out of Phase 0 scope | Needs transfer-inclusive benchmark policy |
| Validation matrix explosion delays implementation | Medium | 1-4 | start with required representative cells and mark other cells `not-started` | New roadmap control | Needs schema and issue-template support |
| Parameter count alone misleads memory feasibility | High | 1-4 | require quantization, KV profile, context, metadata, and backend evidence per validation cell | New roadmap control | Needs validation-matrix manifest fields |
| Cap creep above 16 GiB invalidates current claims | Critical | 1-4 | schema/config validation rejects VRAM tiers above 16 GiB | New roadmap control | Needs `GpuCapPolicy` implementation |
| Cherry-picked model family overgeneralizes bucket result | High | 1-4 | claims bind to exact model hash and bucket; bucket-level generalization requires multiple representatives | New roadmap control | Needs evidence bundle policy |
| Hardware tier unavailable or inconsistent | High | 1-4 | allow `hardware-evidence-absent`; do not promote missing tiers | New roadmap control | Needs validation-cell status tracking |
| Cross-cell comparisons produce false profitability | High | 3 | comparator rejects mismatched bucket/tier/model/context manifests | New roadmap control | Needs benchmark comparator implementation |

## Original Plan Traceability

| Original plan item | Current status | Evidence |
|---|---|---|
| `allocator_tracker.cpp` | Implemented Phase 0 accounting foundation | `CappedAllocatorTracker` tracks current, peak, and rejected attempted peak |
| Forced cap-breach test | Implemented for allocator/process/warmup/unknown paths | `--simulate-*` flags and lifecycle-valid telemetry |
| Cap certification fail-closed behavior | Implemented for current Phase 0 telemetry model | `certify_cap` rejects cap breach, disagreement, missing telemetry, unified memory, and unknown post-warmup allocation |
| Process/device memory reports | Partially implemented | CUDA runtime plus NVML evidence in CUDA-probed build |
| Backend warmup probe | Partially implemented | null/fake warmup passes; process-backed llama.cpp `<=2B` warmup records external evidence and fails closed until allocation reconciliation |

## Research Roadmap Traceability

| Research document | Purpose |
|---|---|
| `docs/research-roadmap-constrained-llm-inference.md` | Phase 1 through Phase 4 constrained-inference roadmap. |
| `docs/validation-matrix.md` | Canonical model-bucket and VRAM-tier validation envelope, capped at 16 GiB. |
| `docs/claim-taxonomy.md` | Canonical claim labels and non-promotion rules. |
| `docs/host-memory-and-io-telemetry.md` | Host RAM, pagefile, mmap, and IO evidence policy. |
| `docs/windows-wddm-telemetry-policy.md` | Windows/NVIDIA driver-mode and telemetry evidence policy. |
| `docs/kv-cache-and-compression-research.md` | KV cache, quantization, PolarQuant/TurboQuant, and quality-gate research notes. |
| `docs/phase1-implementation-plan.md` | Concrete Phase 1 backend adapter and runtime warmup implementation plan. |
| `docs/phase2-implementation-plan.md` | Concrete Phase 2 KV accounting, compression gating, and quality-evidence implementation plan. |
| `docs/phase3-implementation-plan.md` | Concrete Phase 3 transfer-inclusive offload profitability implementation plan. |
| `docs/phase4-implementation-plan.md` | Concrete Phase 4 large-model and 90B claim taxonomy, evidence bundle, and validation implementation plan. |
| `docs/phase4-90b-validation-policy.md` | Large-model and 90B validation policy under the current <=16 GiB cap. |

Archived context:

| Archived document | Purpose |
|---|---|
| `docs/archive/council-constrained-vram-consensus.md` | Historical council consensus superseded by the validation matrix and active phase plans. |
