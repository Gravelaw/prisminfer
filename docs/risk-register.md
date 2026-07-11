# PrismInfer Risk Register

| Risk | Severity | Phase | Mitigation | Current status | Remaining blocker |
|---|---:|---|---|---|---|
| CUDA context consumes most of 1GB | High | 0 | probe before GPU use; fail closed | CUDA context probe and GPU smoke exist | Retain successful self-hosted CUDA artifacts for Phase 0 exit |
| Telemetry disagreement on Windows/WDDM | High | 0 | benchmark mode fails closed; allocator tracker, CUDA runtime, and NVML evidence are reported separately | Disagreement reasons and tolerances are explicit | Review real CUDA artifacts across Class B/C hardware |
| Owned allocation escapes PrismInfer accounting | High | 0/1 | Phase 0 adds capped allocator tracker; Phase 1 adds backend adapter, fake backend, observer, and ledger contracts before real llama.cpp claims | Owned allocation tracker, backend observer, memory ledger, and fake backend breach tests exist | llama.cpp allocation bridge remains required before full governance claims |
| llama.cpp hidden allocations | High | 1 | mandatory backend warmup before certification | process-backed llama.cpp warmup can load pinned `<=2B` and `>2B-5B` GGUF artifacts and records external allocation evidence; certification fails closed on unreconciled external bytes | Reconcile llama.cpp allocation evidence with governed/device memory before certification claims |
| GPU offload slower than CPU | High | 3 | transfer-inclusive profitability rule | Phase 3 profitability policy rejects slower candidates with `offload_not_profitable` | Real hardware artifacts must be retained per validation cell |
| KV compression passes perplexity but fails retrieval | High | 2 | task-level quality gates | deterministic smoke/retrieval/long-context gate model exists; compression without quality evidence fails closed | Real task fixtures and optimized compression kernels remain future work |
| Large-model or 90B hybrid appears feasible but is unusably slow | High | 4 | simulated and validated labels remain separate | Phase 4 usability and repeatability gates reject unusably slow or unstable claims | Real large-model evidence remains external-artifact work |
| Large-model or 90B under <=16 GiB overclassified as deployable | Critical | 4 | enforce claim taxonomy: metadata-only, simulated, measured-offload, validated-benchmark, deployable-profile | Phase 4 claim validator rejects simulated deployable claims and missing evidence | No deployable 90B claim is made |
| Empty or no-model telemetry treated as inference proof | High | 0/1 | distinguish probe harness certification from real backend certification | lifecycle v0.2 records backend selection, dependency pin resolution, model load plan, backend init, and backend warmup; real `<=2B` and `>2B-5B` llama.cpp rejection artifacts exist | Allocation reconciliation still required before certified real-backend governance claims |
| CPU/NVMe offload shifts cap pressure outside VRAM | High | 2/4 | track RSS, commit, mmap residency proxy, pinned host memory, page faults, IO bytes, and pagefile pressure where available | Phase 1 records Windows process working set/private bytes, commit, available memory, and process IO in manifests | Pinned host memory, page faults, mmap residency proxy, cold-cache protocol, and NVMe bytes remain Phase 3/4 work |
| WDDM TDR or driver mode invalidates long-running paths | High | 2/3 | record driver mode, TDR assumptions, and validate long kernels/offload on appropriate driver modes | Not instrumented | Needs driver-mode and watchdog policy |
| PCIe/NVMe roofline invalidates profitability | High | 3/4 | require H2D/D2H bytes per token, NVMe bytes per token, p50/p95 tokens/sec, and CPU-only comparison | Phase 3 manifest v0.4 records transfer metrics and rejects warm-only NVMe evidence | Need broader real hardware evidence before Phase 4 large-model claims |
| Validation matrix explosion delays implementation | Medium | 1-4 | start with required representative cells and mark other cells `not-started` | New roadmap control | Needs schema and issue-template support |
| Parameter count alone misleads memory feasibility | High | 1-4 | require quantization, KV profile, context, metadata, and backend evidence per validation cell | New roadmap control | Needs validation-matrix manifest fields |
| Cap creep above 16 GiB invalidates current claims | Critical | 1-4 | schema/config validation rejects VRAM tiers above 16 GiB | New roadmap control | Needs `GpuCapPolicy` implementation |
| Cherry-picked model family overgeneralizes bucket result | High | 1-4 | claims bind to exact model hash and bucket; bucket-level generalization requires multiple representatives | Phase 4 evidence bundle requires validation cell and artifact evidence | Bucket-level generalization still requires multiple retained representatives |
| Hardware tier unavailable or inconsistent | High | 1-4 | allow `hardware-evidence-absent`; do not promote missing tiers | New roadmap control | Needs validation-cell status tracking |
| Cross-cell comparisons produce false profitability | High | 3 | comparator rejects mismatched bucket/tier/model/context manifests | New roadmap control | Needs benchmark comparator implementation |
| 9B representative gate overgeneralized to the whole `>5B-10B` bucket | High | 2-5 | bind the 9B gate to exact model hash, quantization artifact, context, backend, OS, GPU, driver, CUDA version, and VRAM tier | Proposed roadmap control | Needs retained 9B manifests, quality fixtures, and same-cell comparator evidence |
| Permissive schemas hide invalid kernel evidence | High | 5 | add strict kernel benchmark manifest/schema and reject unknown kernel fields for measured claims | New roadmap control | Needs schema hardening before kernel prototype |
| Baseline mismatch creates false kernel speedup | High | 5 | same-cell comparator covers model, quant, prompt, context, batch, backend, OS, GPU, driver, CUDA version, and cap tier | New roadmap control | Needs benchmark comparator and retained baseline artifacts |
| Custom kernels bypass cap accounting | Critical | 5 | track kernel workspace, retained pools, full-dequant materialization, and unknown allocations; fail closed on unreconciled bytes | New roadmap control | Needs workspace ledger and allocation reconciliation |
| Full FP16 materialization violates constrained-VRAM claim | Critical | 5 | forbid full weight dequantization for constrained kernel claims; record `full_dequant_materialized` | New roadmap control | Needs kernel evidence schema and tests |
| Synthetic quality fixtures overstate kernel correctness | High | 5 | require retained prompt fixture hashes, CPU reference outputs, and tolerance policy | New roadmap control | Needs real quality fixture contract |
| Tensor Core or vendor path claim is unproven | High | 5 | require capability detection, dtype/layout/alignment evidence, fallback reason, and profiler artifact hash | New roadmap control | Needs CUDA feature/profiler evidence contract |
| CLI-only comparator evidence bypasses retained artifact review | High | 6 | require manifest-to-manifest comparison against strict kernel benchmark manifests | New roadmap control | Needs kernel manifest ingestion and required-field validation |
| Toy q4 block semantics misrepresent a mixed GGUF Q4_K_M recipe | High | 6 | enumerate every actual per-tensor `ggml_type` and add exact reference fixtures before performance claims | Tracked by #74; the existing CUDA block remains synthetic only | Needs per-type decoders, eligible-subset mapping and fixture hashes |
| CUDA kernel compile smoke mistaken for CUDA correctness | High | 6 | add guarded device launch correctness test with sync, error checks, copy-back, and CPU-reference tolerance | New roadmap control | Needs self-hosted CUDA correctness workflow |
| 9B artifact or prompt fixture drift invalidates comparisons | High | 6 | pin model hash, quant artifact hash, tokenizer metadata, prompt fixture hash, and baseline manifest hash | New roadmap control | Needs retained artifact store and runbook |
| Compression result overgeneralized across q4 formats or model families | High | 6 | bind compression result to exact model hash, quant artifact, q4 format, context, prompt fixture, backend, GPU, driver, CUDA version, and cap tier | New roadmap control | Needs comparator support for compression variant fields |
| Optimizer emits a plan that the pinned runtime cannot execute safely | Critical | 7 | require an actuator inventory, capability acknowledgement, plan validation, and explicit `R0`-`R3` recovery per decision | Proposed adaptive-runtime control | Needs in-process adapter and acknowledged plan bundle |
| WDDM physical residency is mistaken for PrismInfer-owned allocation | Critical | 7 | keep owned-allocation, process-tree, DXGI budget/usage, backend-ledger, and physical-residency claims separate | Proposed adaptive-runtime control | Needs the Windows evidence protocol and Job-backed process boundary |
| Shell-based backend launch permits unsafe arguments or escapes accounting | Critical | 7 | replace `std::system` with native argument-vector process creation, controlled environment, inherited-handle policy, and Job Object containment | Proposed adaptive-runtime control | Current process-backed adapter remains unsuitable for adaptive execution |
| Calibration overfits one prompt or thermal state | High | 7-9 | randomized blocks, nested holdouts, fresh confirmation runs, abstention, and fingerprint-bound invalidation | Proposed adaptive-runtime control | Needs calibration store, threshold registry, and independent request/session samples |
| Hybrid recurrent state is incorrectly treated as ordinary KV cache | High | 7-8 | use a conventional full-attention 9B foundation cell; model Ornith/DeltaNet state explicitly in a separate stress cell | Proposed adaptive-runtime control | Needs architecture-aware state schema and hybrid fixtures |
| Post-commit router audit is described as lossless fallback | Critical | 8 | require verification before commit for reversible paths; otherwise classify output as explicitly lossy and stop future use | Proposed adaptive-runtime control | Structured-compute provider remains optional research |
| Speculative decoding optimizes acceptance rather than useful output | High | 8 | optimize committed target-distributed output tokens/sec and observed external bytes per committed token | Proposed adaptive-runtime control | Needs exact cycle accounting including the target token and rollback/recovery |
| 70B/90B roadmap work begins despite infeasible host or bandwidth lower bounds | Critical | 7-9 | run exact capacity, storage, transfer, and resource-DAG admission at program entry; keep cells gated when the service envelope cannot be met | Proposed adaptive-runtime control | Needs exact artifacts and measured hardware inputs |
| Nominal 16 GiB policy exceeds the usable WDDM budget or even physical VRAM | Critical | 0/6-9 | treat 16 GiB as a claim ceiling only; derive each run cap from live budget, safety reserve, and owned-allocation evidence | Hardware audit found 16,303 MiB physical VRAM and a smaller usable budget; `AGENTS.md` now blocks fixed-cap long runs | Needs DXGI budget admission, a configurable reserve, and retained preflight evidence |
| CUDA context or workload begins before memory admission and continues after rejection | Critical | 0/6-9 | use conservative pre-context admission, context-only reconciliation, then an exact one-shot workload token; stop immediately on rejection | Tracked by #103; existing probe creates a CUDA context before complete admission and some rejection paths remain observational | #103 implementation and fault-suite closure |
| Size, byte-count, or launch-geometry arithmetic wraps before a safety check | Critical | 0-9 | centralize checked add/multiply/round-up helpers; reject zero, overflow, and out-of-range extents; fuzz boundary values | Several runtime, ledger, backend, and CUDA launch paths require hardening | Needs checked-arithmetic migration plus sanitizer/fuzz evidence |
| Sustained GPU work runs without a hardware supervisor | Critical | 5-9 | single-owner GPU lease, bounded child process, wall-clock deadline, cancellation, thermal/power/VRAM/host-commit watchdog, and fail-closed artifact state | #103 is Ready; `AGENTS.md` prohibits sustained or full-model CUDA work until it closes | #81/#82 prerequisites, #103 implementation and fault-injection tests |
| Windows Device Guard blocks newly built unsigned executables | High | 0/5-9 | retain Code Integrity evidence, perform a harmless launch check before GPU access, and use an approved signing/trust path; never weaken Device Guard | Historical Code Integrity events showed blocked PrismInfer test executables; the current probe launches | Needs a repeatable trusted-build/signing procedure for self-hosted validation |
| Untrusted pull-request code executes on a persistent self-hosted GPU runner | Critical | 0-9 | keep GPU workflows manual-only on trusted refs, disable checkout credential persistence, minimize token permissions, and retain runner provenance | Local workflow changes remove pull-request triggers and persistent checkout credentials | Needs review/push plus an ephemeral or freshly restored runner policy |

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
| `Plan.md` | Canonical final thesis, clearance matrix, dependency order, current Project status and phase outcomes. |
| `docs/research-roadmap-constrained-llm-inference.md` | Detailed Phase 1-6 research history plus the safety-first static-controller handoff governed by `Plan.md`. |
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
| `docs/phase5-compute-kernel-research-plan.md` | Concrete Phase 5 measured compute-kernel research plan and stop/go gates. |
| `docs/phase6-implementation-plan.md` | Concrete Phase 6 9B evidence and kernel validation plan. |
| `docs/phase6-compression-architecture.md` | Phase 6 compression workflow, memory ledger, lane order, and claim classification. |
| `docs/phase6-evidence.md` | Phase 6 evidence status and audit placeholder. |
| `docs/kernel-benchmark-methodology.md` | Benchmark, comparator, baseline, and profiler evidence policy for kernel work. |
| `docs/adaptive-runtime/README.md` | Index for the proposed calibrated adaptive-runtime architecture, 9B optimizer experiment, evidence protocols, scope, roadmap, and council record. |

Archived context:

| Archived document | Purpose |
|---|---|
| `docs/archive/council-constrained-vram-consensus.md` | Historical council consensus superseded by the validation matrix and active phase plans. |
