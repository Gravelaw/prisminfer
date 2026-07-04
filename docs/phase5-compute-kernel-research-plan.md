# Phase 5 Implementation Plan: Measured Compute Kernel Research

Phase 5 includes CUDA kernel implementation as a gated research deliverable after
the Phase 1-4 claim, memory, KV, offload, and large-model gates. It authorizes
exactly one guarded prototype first: fixed-block q4 fused dequantization plus
matmul for batch-1 decode GEMV. It does not authorize a general CUDA runtime
rewrite, attention kernels, broad GEMM coverage, or deployable-profile claims
without the Phase 5 evidence gates.

## Goal

PrismInfer should be able to compare a candidate compute kernel against CPU,
llama.cpp/GGML, and vendor-library baselines inside the same validation cell,
while preserving hard-cap memory accounting and quality evidence.

The required Phase 5 prototype target is a fixed-block q4 fused dequantization
plus matmul path for batch-1 decode GEMV. It is implemented only after the entry
gates below pass.

## Non-Goals

- No custom attention kernel in the first Phase 5 slice.
- No MLA latent KV runtime implementation.
- No low-rank/SVD or structured-sparsity runtime compression.
- No MoE runtime implementation.
- No Tensor Core claim without feature, layout, alignment, and profiler
  evidence.
- No deployable-profile claim.
- No hard-cap certification for llama-backed kernel runs while backend
  allocations remain unreconciled.
- No materialization of full FP16 weights in VRAM for constrained-memory claims.

## Entry Gates

Kernel implementation cannot begin until these gates pass:

1. Strict kernel evidence schema exists.
   Unknown kernel fields must be rejected or quarantined so a measured kernel
   claim cannot be made from misspelled or absent fields.
2. Same-cell benchmark comparator exists.
   Comparisons must reject mismatched model hash, quantization, prompt fixture,
   context length, batch size, backend, OS, GPU, driver, CUDA version, and cap
   tier.
3. Real quality fixture contract exists.
   Kernel speed claims must include retained prompt fixture hashes, CPU
   reference outputs or classes, tolerance policy, and token-count policy.
4. Windows runtime blockers are instrumented.
   WDDM/TCC, TDR, and WDAC/Application Control status must be recorded or
   explicitly unavailable. A blocked executable is not a pass.
5. Allocation reconciliation status is explicit.
   llama.cpp/backend/process/device/workspace/KV/retained-pool bytes must be
   reconciled for certification. If they are not reconciled, results are
   `measured-non-certified`.
6. Baselines exist for the same validation cell.
   Required baselines are CPU reference and llama.cpp/GGML CUDA/MMQ when CUDA is
   involved. GEMM and Tensor Core claims also require cuBLASLt or CUTLASS
   baselines where available.
7. 9B constrained-VRAM validation cell is declared.
   Phase 5 must declare at least one exact `>5B-10B` / 9B-class model cell
   before P5-08 begins. The cell must bind model id, model hash, quantization,
   context, batch size, prompt fixture, backend, OS, GPU, CUDA version, and a
   hard VRAM cap no greater than 8 GiB unless the test is explicitly classified
   as a lower-cap rejection. A rejected impossible-math or cap-accounting result
   is valid evidence; it is not a performance claim.

## Build Contract

Add Phase 5 build options:

```text
PRISMINFER_ENABLE_CUDA_KERNELS=OFF
PRISMINFER_CUDA_KERNEL_ARCHS=<explicit CMAKE_CUDA_ARCHITECTURES list>
PRISMINFER_ENABLE_CUBLASLT_BASELINE=ON when CUDA kernels are enabled and cuBLASLt is available
PRISMINFER_ENABLE_CUTLASS_BASELINE=OFF unless a pinned CUTLASS dependency is declared
```

Rules:

- This option is separate from `PRISMINFER_ENABLE_CUDA_PROBE`.
- Kernel builds require CUDA language/NVCC, not only the CUDA-probe runtime-link
  fallback.
- Hosted non-CUDA CI must remain valid.
- Kernel targets under guarded `src/kernels/` compile only when the option is
  explicitly enabled.
- Self-hosted CUDA workflows own kernel evidence.
- Manifests record kernel flag, CUDA arch list, toolkit/runtime/driver,
  cuBLASLt/CUTLASS availability, and fallback reasons.
- A kernel PR that makes ordinary probe, validation, or non-CUDA tests depend on
  CUDA kernels is rejected.

## Phase 5 Slices

| Slice | Work | Candidate files | Exit gate |
|---|---|---|---|
| P5-01 | Strict kernel evidence schema | `schemas/kernel_benchmark_manifest.schema.json`, `schemas/telemetry.schema.json` | Unknown kernel fields cannot silently promote measured claims. |
| P5-02 | Same-cell benchmark comparator | `include/prisminfer/benchmark_comparator.h`, `src/benchmark/benchmark_comparator.cpp`, `tools/prism-compare-benchmark/main.cpp` | Mismatched cells are rejected with precise reasons. |
| P5-03 | Quality fixture contract | `configs/`, `tools/prism-quality`, `schemas/evidence_bundle.schema.json` | Speed claims require retained fixture hashes and CPU reference/tolerance evidence. |
| P5-04 | Windows runtime evidence | `docs/windows-wddm-telemetry-policy.md`, CUDA probe telemetry, manifests | WDDM/TDR/WDAC/Application Control states are recorded or classified. |
| P5-05 | Kernel build gate | `CMakeLists.txt`, self-hosted CUDA workflows | `PRISMINFER_ENABLE_CUDA_KERNELS` is off by default and isolated. |
| P5-06 | Allocation reconciliation contract | `backend_memory_observer`, `memory_ledger`, manifests | Unknown backend/workspace bytes fail closed or force `measured-non-certified`. |
| P5-07 | GEMV/GEMM/vendor policy | `docs/kernel-benchmark-methodology.md` | Decode GEMV, prefill GEMM, cuBLASLt/CUTLASS, and Tensor Core policies are explicit. |
| P5-07G | 9B constrained-VRAM matmul gate | `docs/validation-matrix.md`, `docs/kernel-benchmark-methodology.md`, retained 9B manifests | A pinned 9B `>5B-10B` q4 cell is declared for `context_tokens=2048`, `batch_size=1`, and same-tier comparison. |
| P5-08 | Implement one fixed-block q4 fused dequant+GEMV CUDA prototype | `src/kernels/cuda/`, `include/prisminfer/kernels/`, guarded tests | Compiles only with `PRISMINFER_ENABLE_CUDA_KERNELS=ON`; no full FP16 materialization; CPU correctness passes. |
| P5-09 | 9B constrained-VRAM kernel gate | external model artifact, manifest, telemetry, comparator output | At least one exact 9B-class cell under <=8 GiB is measured or rejected with retained evidence; no bucket/general 9B claim from one model. |
| P5-10 | Phase 5 evidence and audit | `docs/phase5-evidence.md`, `docs/risk-register.md` | Prototype is measured, rejected, or kept research-only with retained artifacts. |

## Kernel Telemetry Fields

Phase 5 kernel telemetry should include:

```text
kernel_lane
kernel_name
kernel_version
kernel_backend
op_type
sequence_phase
batch_size
matrix_m
matrix_n
matrix_k
quant_format
block_size
bits_per_weight
dequant_fused
full_dequant_materialized
workspace_bytes
workspace_peak_bytes
kernel_ms
launch_ms
sync_ms
transfer_h2d_bytes
transfer_d2h_bytes
achieved_bandwidth_gbps
achieved_tflops
tensor_core_claimed
tensor_core_used
fallback_reason
correctness_fixture_hash
correctness_max_abs_error
correctness_max_rel_error
baseline_backend
baseline_manifest_hash
speedup_ratio
claim_status
```

Tensor Core, occupancy, stall, L2, and DRAM claims require profiler artifact
paths and hashes. If profiler evidence is missing, the claim must be phrased as
ordinary measured wall-clock performance, not hardware-path proof.

## Missing Ideas Placement

| Idea | Phase 5 placement |
|---|---|
| Fused dequant plus matmul | P5-08 required CUDA prototype target after gates. |
| GEMM versus GEMV kernel selection | P5-07 policy; decode batch-1 GEMV first, prefill GEMM via vendor baselines. |
| CUTLASS, cuBLASLt, Tensor Core strategy | P5-07 vendor baseline lane before custom kernels. |
| FlashAttention-style IO-aware kernels | Later attention lane after Phase 5 matmul evidence. |
| MLA-style latent KV | Later KV/attention lane after accounting and quality fixtures. |
| Low-rank/SVD/structured sparsity | Accounting and artifact metadata first, no runtime compression in Phase 5. |
| MoE active-parameter accounting | Schema/accounting lane first, no MoE runtime in Phase 5. |

## Stop and Go Gates

Allowed before P5-08:

- docs,
- strict schemas,
- benchmark comparator,
- quality fixture contract,
- Windows runtime evidence fields,
- CUDA kernel build isolation,
- allocation reconciliation design,
- CPU-reference correctness harness,
- design-only kernel interfaces.

Stop now:

- Tensor Core acceleration claims,
- deployable-profile claims,
- second kernel paths,
- attention kernels,
- MLA runtime,
- MoE runtime,
- 70B/90B measured kernel claims.

Go for one CUDA prototype only when P5-01 through P5-07G pass.

9B matmul optimization evaluation:

- evaluate batch-1 decode GEMV first, not prefill GEMM,
- compare only within the same 9B validation cell: model hash, quantization,
  prompt fixture, context, batch, backend, OS, GPU, driver, CUDA version, and
  VRAM tier must match,
- required baselines: CPU reference correctness, no-custom PrismInfer run, and
  llama.cpp/GGML CUDA/MMQ for the same GGUF artifact,
- cuBLASLt or CUTLASS is required only when making GEMM, Tensor Core, or vendor
  baseline claims,
- isolated kernel speedups are diagnostic only; promotion requires at least
  `10%` faster end-to-end decode versus same-cell llama.cpp/GGML CUDA/MMQ,
- added kernel workspace must be recorded and must not push peak VRAM above the
  tier cap,
- reject the prototype if `full_dequant_materialized=true`, correctness fails,
  quality fixture evidence is missing, cap accounting is unreconciled, or the
  end-to-end decode gain is below `10%`.

Abandon or revise the prototype if it:

- fails correctness against CPU reference,
- loses end-to-end decode by more than 10% versus llama.cpp/GGML CUDA/MMQ,
- needs multiple extra kernels to become viable,
- requires full FP16 materialization in VRAM,
- fails cap accounting,
- lacks same-cell baseline artifacts,
- lacks quality fixture evidence.
