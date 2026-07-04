# Implementation Plan: Phase 1 Through Phase 5

This plan turns the constrained-VRAM research roadmap into SDLC slices for the
GitHub tracker.

Current baseline checked against `Gravelaw/prisminfer`:

- Default branch: `main`.
- Local checkout path: `D:\Research\prisminfer`.
- Current local head during the Phase 5 planning pass: `9dffea7`.
- Phase 0 issues `#1` through `#15` are closed.
- No open issues remain under the Phase 0 milestone at the time of this pass.

## GitHub Tracker Updates

Before Phase 1 implementation, update project tracking:

1. Close or archive the Phase 0 milestone after the audit state is accepted.
2. Create milestones:
   - `Phase 1: Backend Adapter and Real Warmup Evidence`
   - `Phase 2: KV Cache Accounting and Compression Gates`
   - `Phase 3: Transfer-Inclusive Offload Profitability`
   - `Phase 4: Large-Model and 90B Hybrid/Offload Validation`
   - `Phase 5: Measured Compute Kernel Research`
3. Add labels:
   - `phase:1`, `phase:2`, `phase:3`, `phase:4`, `phase:5`
   - `area:backend`, `area:gguf`, `area:llama.cpp`,
     `area:memory-ledger`, `area:kv-cache`, `area:quality`,
     `area:offload`, `area:profitability`, `area:claim-taxonomy`,
     `area:kernels`, `area:benchmark-comparator`
   - `gate:phase1-backend-evidence`, `gate:phase2-quality`,
     `gate:phase3-profitability`, `gate:phase4-validation`,
     `gate:phase5-kernel-evidence`
4. Rename `PrismInfer Phase 0` to `PrismInfer Roadmap`, or create a new roadmap
   project if preserving the Phase 0 board matters.
5. Extend project fields:
   - `Slice`: Backend Adapter, GGUF/GGML, Memory Ledger, KV Cache,
     Quality Gates, Offload, Profitability, Validation Matrix,
     Large-Model Validation, Claim Taxonomy, Kernel Evidence,
     Benchmark Comparator.
   - `Gate`: add the phase-specific gates above.
   - Optional `Claim Class`: Scaffold, Observed, Governed, Simulated,
     Validated, Rejected.

## Validation Matrix Gate

Before Phase 1 implementation proceeds, add the validation-matrix fields to
schemas, configs, manifests, and GitHub issue templates:

```text
model_parameter_bucket
parameter_count
vram_tier_gib
hard_vram_cap_bytes
validation_cell_id
validation_cell_status
```

Current maximum GPU hard cap:

```text
16 GiB = 17179869184 bytes
```

No issue or claim should require a larger GPU tier until cap policy is
explicitly changed.

## Phase 1: Backend Adapter and Real Warmup Evidence

Exit criterion:

PrismInfer can load or warm a real GGUF through llama.cpp, emit lifecycle-valid
telemetry and manifests with backend allocation evidence, and fail closed when
backend allocation evidence is missing, contradictory, or over cap.

| Slice | Issue | Candidate files/modules | Gate |
|---|---|---|---|
| P1-00 | Enforce max VRAM cap and validation-cell schema | `include/prisminfer/gpu_cap_policy.h`, `schemas/*`, `docs/validation-matrix.md` | Caps above 16 GiB rejected. |
| P1-01 | Pin llama.cpp/GGML/GGUF dependency and build mode | `CMakeLists.txt`, `docs/dependency-pins.md`, `third_party/llama.cpp-pin.json` | Exact commit/hash and integration mode recorded. |
| P1-02 | Define backend adapter contract | `include/prisminfer/backend_adapter.h`, `src/backend/backend_adapter.cpp` | Unit-tested fake adapter. |
| P1-03 | Implement memory observer and backend ledger | `include/prisminfer/backend_memory_observer.h`, `include/prisminfer/memory_ledger.h`, `src/governor/memory_ledger.cpp` | PrismInfer, backend, KV, workspace, retained, and unknown bytes separated. |
| P1-04 | Add minimum host/OS telemetry | `include/prisminfer/host_memory_tracker.h`, `docs/host-memory-and-io-telemetry.md` | Host pressure fields recorded or explicitly unavailable. |
| P1-05 | Implement llama.cpp adapter | `src/backend/llama_backend_adapter.cpp` | Real init, model load, warmup, free. |
| P1-06 | Replace placeholder `backend_warmup` for backend-enabled runs | `tools/prism-probe/main.cpp`, `include/prisminfer/runtime_state.h` | Placeholder not used for real backend certification. |
| P1-07 | Extend telemetry and manifest schemas to v0.2 | `schemas/telemetry.schema.json`, `schemas/benchmark_manifest.schema.json`, `src/benchmark/manifest_writer.cpp` | Backend commit, model hash, validation cell, and warmup peaks recorded. |
| P1-08 | Add backend lifecycle validation | `tools/prism-validate-lifecycle/main.cpp` | Rejects missing real backend events. |
| P1-09 | Add self-hosted backend evidence workflow | `.github/workflows/backend-evidence-self-hosted.yml` | Real GGUF warmup artifacts uploaded. |
| P1-10 | Phase 1 exit audit | `docs/phase1-evidence.md`, `docs/risk-register.md` | All P1 blockers resolved or classified. |

Test gates:

- `scripts\verify.ps1`
- `scripts\verify.ps1 -WithCuda`
- `PRISMINFER_ENABLE_LLAMA_BACKEND=ON` build plus CTest.
- `PRISMINFER_TEST_GGUF=<path-to-small-gguf>` for local/self-hosted real model
  evidence only.

Claim rule:

Until llama.cpp allocation paths are bridged or fully reconciled, call the
evidence `observed backend allocation evidence`, not `full governed backend
allocation`.

## Phase 2: KV Cache Accounting and Quality-Gated Compression

Exit criterion:

PrismInfer can account for KV cache growth by prompt/context/token phase and
evaluate compression only through quality gates.

| Slice | Issue | Candidate files/modules | Gate |
|---|---|---|---|
| P2-01 | KV cache ledger and lifecycle events | `include/prisminfer/kv_cache_ledger.h`, `src/kv/kv_cache_ledger.cpp` | Per-token/context KV bytes recorded. |
| P2-02 | Add token/prefill/decode telemetry | `include/prisminfer/runtime_state.h`, `schemas/telemetry.schema.json` | `prefill_start`, `prefill_end`, `decode_token`, `kv_cache_sample`. |
| P2-03 | Baseline uncompressed KV measurements | `docs/phase2-kv-baseline.md` | Baseline by validation cell. |
| P2-04 | Quality harness | `tools/prism-quality/main.cpp`, `src/quality/quality_runner.cpp` | Perplexity plus task-level retrieval gates. |
| P2-05 | Compression policy interface | `include/prisminfer/kv_compression_policy.h` | Reference/stub policies only at first. |
| P2-06 | KIVI/KVQuant research prototype lane | `src/kv/policies/` | Experimental until quality pass. |
| P2-07 | PolarQuant/TurboQuant/QJL literature lane | `docs/kv-cache-and-compression-research.md` | Decision record before implementation claim. |
| P2-08 | Phase 2 exit audit | `docs/phase2-evidence.md` | Cell-scoped memory reduction plus quality acceptance. |

Quality gates:

- Perplexity delta.
- Long-context retrieval.
- Instruction-following smoke.
- Deterministic regression prompts.
- Attention-logit and top-k diagnostics where available.

Any compression that only passes perplexity but fails retrieval is rejected for
Phase 2.

## Phase 3: Transfer-Inclusive GPU/Offload Profitability

Exit criterion:

GPU/offload paths are accepted only when total wall-clock profitability includes
staging, transfer, synchronization, load, warmup, prefill, decode, and cleanup.

| Slice | Issue | Candidate files/modules | Gate |
|---|---|---|---|
| P3-01 | Transfer/staging telemetry | `src/benchmark/transfer_timer.cpp`, `include/prisminfer/transfer_metrics.h` | PCIe/offload timings recorded. |
| P3-02 | Offload planner contract | `include/prisminfer/offload_planner.h`, `src/offload/offload_planner.cpp` | Plans are explainable and bounded. |
| P3-03 | CPU baseline benchmark suite | `configs/benchmark-cpu-baseline.json` | Same validation cell as offload runs. |
| P3-04 | GPU/offload benchmark suite | `configs/benchmark-offload-profitability.json` | Transfer-inclusive p50/p95 metrics. |
| P3-05 | Profitability policy | `src/governor/profitability_policy.cpp` | Rejects slower offload paths. |
| P3-06 | CI/self-hosted profitability evidence | `.github/workflows/offload-profitability-self-hosted.yml` | Artifacts retained. |
| P3-07 | Phase 3 exit audit | `docs/phase3-evidence.md` | Offload claim allowed only with same-cell measured win. |

## Phase 4: Large-Model and 90B Hybrid/Offload Validation

Exit criterion:

Large-model support, including the terminal 90B bucket, is either validated with
retained evidence or explicitly classified as simulated/offload-only. No
deployability claim is allowed from extrapolation.

| Slice | Issue | Candidate files/modules | Gate |
|---|---|---|---|
| P4-01 | Claim taxonomy | `docs/claim-taxonomy.md`, `configs/90b-hybrid-offline.json` | Simulated, measured, validated, deployable labels enforced under <=16 GiB. |
| P4-02 | Hybrid/offload planning model | `include/prisminfer/hybrid_plan.h`, `src/planner/hybrid_plan.cpp` | Memory and transfer budget explicit. |
| P4-03 | Large-model evidence manifest schema | `schemas/benchmark_manifest.schema.json` | Model, quant, shards, hardware, prompts, latency, validation cell. |
| P4-04 | 90B dry-run/simulation planner | `tools/prism-plan-90b/main.cpp` | Always labeled simulated. |
| P4-05 | 90B measured offload run | self-hosted/manual evidence workflow | External model artifact, no committed weights. |
| P4-06 | Usability threshold policy | `src/governor/usability_policy.cpp` | Rejects technically feasible but unusably slow runs. |
| P4-07 | Phase 4 exit audit | `docs/phase4-evidence.md` | Claim taxonomy and impossible-math gate enforced. |

## Phase 5: Measured Compute Kernel Research

Exit criterion:

PrismInfer can decide whether one narrow compute-kernel prototype is worth
owning by comparing it against CPU reference, llama.cpp/GGML CUDA/MMQ, and
vendor-library baselines in the same validation cell. Phase 5 does not grant a
deployable-profile claim and does not permit broad kernel implementation. It
does include one gated CUDA kernel implementation slice after schema,
comparator, fixture, runtime, 9B validation-cell, and allocation gates pass.

| Slice | Issue | Candidate files/modules | Gate |
|---|---|---|---|
| P5-01 | Add strict kernel evidence contract | `schemas/kernel_benchmark_manifest.schema.json`, `schemas/telemetry.schema.json` | Unknown kernel fields are rejected or explicitly quarantined. |
| P5-02 | Implement same-cell benchmark comparator | `include/prisminfer/benchmark_comparator.h`, `src/benchmark/benchmark_comparator.cpp`, `tools/prism-compare-benchmark/main.cpp` | Mismatched model, quant, context, backend, OS, GPU, driver, CUDA, batch, or cap tier is rejected. |
| P5-03 | Add real quality fixture contract | `configs/`, `schemas/evidence_bundle.schema.json`, `tools/prism-quality/main.cpp` | Kernel speed claims require retained prompt/expected-output/tolerance hashes. |
| P5-04 | Instrument Windows runtime blockers | `docs/windows-wddm-telemetry-policy.md`, CUDA probe and manifest fields | WDDM/TCC, TDR, and WDAC/Application Control states are recorded or classified unavailable. |
| P5-05 | Add CUDA kernel build contract | `CMakeLists.txt`, `.github/workflows/*self-hosted*.yml` | `PRISMINFER_ENABLE_CUDA_KERNELS=OFF` by default and separate from CUDA probe. |
| P5-06 | Reconcile backend allocation evidence | `backend_memory_observer`, `memory_ledger`, manifests | Unknown llama/backend/workspace bytes fail closed or force `measured-non-certified`. |
| P5-07 | Design GEMV/GEMM/vendor baseline policy | `docs/kernel-benchmark-methodology.md` | Batch-1 decode GEMV, prefill GEMM, cuBLASLt/CUTLASS/Tensor Core assumptions are explicit. |
| P5-07G | Add representative 9B constrained-VRAM gate | `docs/validation-matrix.md`, `configs/9b-constrained-kernel-gate.json` | A pinned 9B-class `>5B-10B` q4 cell is declared before prototype promotion. |
| P5-08 | Implement one fixed-block q4 fused dequant+decode-GEMV CUDA path | guarded `src/kernels/cuda/` tree | Allowed only after P5-01 through P5-07G pass; compile-gated with `PRISMINFER_ENABLE_CUDA_KERNELS=ON`; no full FP16 materialization. |
| P5-09 | Run the 9B constrained-VRAM kernel gate | retained external 9B manifests and comparator output | Exact 9B cell under <=8 GiB is measured or rejected; no bucket-level 9B claim. |
| P5-10 | Phase 5 exit audit | `docs/phase5-evidence.md` | Kernel claim is measured, rejected, or left research-only with retained artifacts. |

Placement of current compute ideas:

- Fused dequant plus matmul: P5-08, one fixed-block q4 decode-GEMV CUDA
  prototype only.
- GEMM versus GEMV selection: P5-07 policy before implementation.
- CUTLASS, cuBLASLt, Tensor Core strategy: P5-07 vendor baseline lane before
  custom kernels.
- FlashAttention-style IO-aware kernels: later attention lane after Phase 5
  matmul evidence.
- MLA-style latent KV: later KV/attention lane after accounting and quality
  fixtures.
- Low-rank/SVD and structured sparsity: accounting/schema lane first.
- MoE active-parameter accounting: schema/accounting lane first; no MoE runtime
  in Phase 5.

## First Implementation Order

1. Update GitHub milestones, labels, project fields, and automation language from
   Phase 0-only to roadmap-wide.
2. Create Phase 1 issues P1-00 through P1-10.
3. Make P1-00, P1-01, P1-02, and P1-03 the first active issues.
4. Add dependency pin design for llama.cpp before touching adapter code.
5. Implement the backend adapter interface with a fake adapter and unit tests.
6. Add the memory ledger abstraction and schema fields while keeping Phase 0
   probe behavior green.
7. Integrate llama.cpp behind `PRISMINFER_ENABLE_LLAMA_BACKEND`.
8. Replace placeholder `backend_warmup` only for backend-enabled runs.
9. Add backend evidence workflow and artifact retention.
10. Run Phase 1 exit audit before opening Phase 2 implementation.
11. Before opening Phase 5 kernel code, complete strict schemas, comparator,
    quality fixtures, Windows runtime evidence, 9B validation-cell declaration,
    and allocation reconciliation.
