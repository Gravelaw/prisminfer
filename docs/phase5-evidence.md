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
- `nvcc -std=c++20 -I include -ccbin <MSVC Hostx64 x64 bin> -c
  src\kernels\cuda\q4_decode_gemv.cu -o %TEMP%\q4_decode_gemv.obj`
  compiled the guarded CUDA source successfully.
- `cmake -S . -B build-vs2026-plain-cuda-check -G "Visual Studio 18 2026"
  -A x64 -DPRISMINFER_ENABLE_CUDA_KERNELS=ON
  -DPRISMINFER_CUDA_KERNEL_ARCHS=120` configured CUDA language support
  successfully with CUDA 13.3 and MSVC 19.51.
- `cmake --build build-vs2026-plain-cuda-check --config Debug --target
  prisminfer_cuda_kernels --parallel` compiled
  `src\kernels\cuda\q4_decode_gemv.cu` into `prisminfer_cuda_kernels.lib`.
- `cmake --preset vs2026-cuda-sm120` and
  `cmake --build --preset vs2026-cuda-sm120` preserve that local CUDA kernel
  verification path for VS Code and command-line CMake users. The preset
  requires CMake 4.2 or newer for the `Visual Studio 18 2026` generator.

Local CUDA kernel compile note:

This machine has `nvcc` from CUDA 13.3 and reports an RTX 5080 Laptop GPU with
compute capability `12.0`. Direct `nvcc` compilation succeeds when MSVC
`cl.exe` is supplied with `-ccbin`. After reinstalling Visual Studio 2026 Build
Tools, Visual Studio 2026 Community, and CUDA Toolkit 13.3, CUDA MSBuild
integration is registered under the VS 2026 BuildCustomizations directories.
Plain `cmake -G "Visual Studio 18 2026"` CUDA-kernel configure now succeeds.
Visual Studio 2022 is not the preferred local CUDA kernel generator for this
machine because its CUDA MSBuild integration is not registered. The default
non-CUDA build is unaffected.

Disallowed claims:

- deployable-profile inference from Phase 5 alone,
- Tensor Core acceleration without profiler artifact hashes,
- batch-1 decode GEMV speedup generalized to prefill GEMM,
- any 9B bucket-wide claim from one representative cell,
- constrained-VRAM claims when full FP16 weights are materialized in VRAM.
