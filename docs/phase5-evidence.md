# Phase 5 Evidence

Phase 5 adds measured compute-kernel research gates. It does not make a
deployable-profile claim.

Implemented gates:

- strict kernel benchmark manifest schema with `additionalProperties: false`,
- same-cell benchmark comparator and `prism-compare-benchmark`,
- kernel evidence validator for correctness, quality fixture, same-cell
  baseline, allocation reconciliation, full-dequant rejection, Tensor Core
  profiler evidence, and minimum speedup,
- isolated CUDA kernel build option `PRISMINFER_ENABLE_CUDA_KERNELS=OFF` by
  default,
- one guarded fixed-block q4 fused dequantization plus batch-1 decode GEMV CUDA
  source path, compiled only when CUDA kernels are explicitly enabled,
- CPU reference correctness harness for the q4 decode-GEMV path,
- representative 9B constrained-VRAM validation cell config:
  `configs/9b-constrained-kernel-gate.json`.

Current claim:

Phase 5 is implemented as a gated research lane. No real 9B model artifact,
same-cell llama.cpp/GGML CUDA/MMQ baseline, or measured kernel benchmark is
retained in this repository. Therefore the current Phase 5 kernel status is
`research-only` until external model artifacts and self-hosted CUDA evidence are
retained.

Verification:

- `cmake -S . -B build`
- `cmake --build build --config Debug --parallel`
- `ctest --test-dir build -C Debug --output-on-failure`
- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1`
- `python` JSON parse over `schemas/*.json` and `configs/*.json`
- `prism-compare-benchmark` accepted a same-cell fixture and rejected a
  mismatched model hash with `model_hash_mismatch`.
- `cmake -S . -B build-kernel-gate-check -DPRISMINFER_ENABLE_CUDA_KERNELS=ON`
  failed closed with
  `PRISMINFER_ENABLE_CUDA_KERNELS requires PRISMINFER_CUDA_KERNEL_ARCHS`.

Local CUDA kernel compile note:

This machine has `nvcc` from CUDA 13.3 and reports an RTX 5080 Laptop GPU with
compute capability `12.0`. The Visual Studio CMake generator still failed CUDA
language enablement with `No CUDA toolset found`, so the guarded `.cu` source
was not locally compiled in this pass. The default non-CUDA build is unaffected,
and kernel compilation remains owned by explicit CUDA-kernel build jobs with an
installed CUDA CMake toolset.

Disallowed claims:

- deployable-profile inference from Phase 5 alone,
- Tensor Core acceleration without profiler artifact hashes,
- batch-1 decode GEMV speedup generalized to prefill GEMM,
- any 9B bucket-wide claim from one representative cell,
- constrained-VRAM claims when full FP16 weights are materialized in VRAM.
