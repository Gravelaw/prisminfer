# Kernel Benchmark Methodology

This document defines how PrismInfer should benchmark compute-kernel research
without overstating constrained-VRAM claims.

## Benchmark Principle

A kernel benchmark is valid only inside one validation cell. The benchmark must
name the model hash, quantization format, context length, prompt fixture, batch
size, sequence phase, backend, OS, GPU, driver, CUDA runtime/toolkit version,
and VRAM cap tier.

Comparisons across cells are rejected. A faster batch-1 decode GEMV result does
not imply a faster prefill GEMM result. A result on one GPU architecture or
driver mode does not imply another.

## Required Baselines

Every kernel candidate needs:

- CPU reference correctness,
- llama.cpp/GGML baseline for the same model and quantization,
- llama.cpp/GGML CUDA/MMQ baseline when CUDA is involved,
- cuBLASLt or CUTLASS baseline for GEMM or Tensor Core claims where available,
- a no-custom-kernel PrismInfer run to prove the measurement harness itself is
  not the source of the difference.

## Measurement Contents

Record:

- warmup protocol,
- cold versus warm cache status,
- kernel wall-clock time,
- launch time,
- synchronization time,
- H2D and D2H transfer bytes and time,
- dequantization mode and cost,
- workspace current and peak bytes,
- retained pools,
- output correctness error,
- quality fixture result,
- profiler artifact path and hash when making hardware-path claims.

The benchmark must include end-to-end decode or prefill timing. An isolated
kernel timing can support diagnosis, but it cannot promote a performance claim.

## GEMV and GEMM Policy

Decode at batch 1 is expected to be memory-bandwidth-sensitive GEMV. The first
custom kernel is decode-only: `sequence_phase=decode`, `batch_size=1`, q4
fixed-block fused dequantization plus GEMV. It must not promote prefill,
batch >1, GEMM, or Tensor Core claims.

Prefill and larger batches are GEMM-like. They require cuBLASLt or CUTLASS
baselines before any custom GEMM is proposed. A custom GEMM is a later gate, not
part of the first P5-08 prototype.

## Tensor Core Policy

Do not claim Tensor Core acceleration unless all are true:

- the GPU capability supports the path,
- dtype, layout, and alignment are eligible,
- fallback reason is absent,
- profiler evidence shows the expected Tensor Core path,
- the profiler artifact path and hash are retained.

If any condition is missing, classify the run as ordinary CUDA wall-clock
evidence, not Tensor Core evidence.

## Memory Policy

Kernel candidates must not materialize the full FP16 weight matrix in VRAM for a
constrained-memory claim. Dequantization should be tiled into registers or
shared memory and measured as part of the kernel path. Writing a full FP16
matrix to global VRAM makes the constrained-memory claim invalid for that run.

Certification requires reconciled PrismInfer allocator, backend/process, CUDA
device, workspace, KV, retained-pool, and unknown allocation telemetry. If
unknown bytes remain, the result is `measured-non-certified` or rejected.

## Promotion Rule

A kernel prototype can be kept only when:

- correctness passes against CPU reference,
- quality fixture evidence passes,
- same-cell baselines are retained,
- cap accounting is certified or explicitly labeled non-certified,
- end-to-end decode or prefill improves by at least 10% over the same-cell
  llama.cpp/GGML CUDA/MMQ baseline.

Otherwise the kernel result is rejected or retained as research-only evidence.
