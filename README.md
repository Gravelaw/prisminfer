# PrismInfer

PrismInfer is a low-VRAM LLM inference governor, telemetry harness, and
research scaffold for constrained GPU memory claims.

The v0 direction is intentionally conservative:

- use llama.cpp/GGML/GGUF compatibility rather than a clean-sheet runtime,
- start with `1gb-safe-cpu`,
- allow GPU only through `1gb-safe-gpu-probed`,
- fail closed when hard-cap telemetry is missing or contradictory,
- include one gated CUDA kernel prototype in Phase 5 after the benchmark,
  schema, runtime evidence, 9B validation-cell, and allocation-reconciliation
  gates pass.

## Build

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

CUDA probing is optional and uses a separate build directory:

```powershell
cmake -S . -B build-cuda -DPRISMINFER_ENABLE_CUDA_PROBE=ON
cmake --build build-cuda --config Debug
ctest --test-dir build-cuda -C Debug --output-on-failure
```

CUDA kernel prototype builds are gated separately from probe support. On
Windows, the checked-in CMake preset targets Visual Studio 2026 and an sm_120
GPU. It requires CMake 4.2 or newer because that CMake release added the
`Visual Studio 18 2026` generator:

```powershell
cmake --preset vs2026-cuda-sm120
cmake --build --preset vs2026-cuda-sm120
```

For a different CUDA GPU, copy the preset locally or configure manually with
the matching `PRISMINFER_CUDA_KERNEL_ARCHS` value.

## Probe

```powershell
.\build\Debug\prism-probe.exe --mode 1gb-safe-cpu --telemetry probe.jsonl
```

Without CUDA probe support compiled in, GPU-probed mode is expected to fail closed:

```powershell
.\build\Debug\prism-probe.exe --mode 1gb-safe-gpu-probed
```

Each probe writes a JSONL telemetry file and a minimal manifest. Use `--run-id`, `--telemetry`, and `--manifest` to make evidence paths explicit:

```powershell
.\build\Debug\prism-probe.exe --mode 1gb-safe-cpu --run-id cpu-smoke --telemetry probe.jsonl --manifest manifest.json
```

Validate a probe lifecycle:

```powershell
.\build\Debug\prism-validate-lifecycle.exe probe.jsonl
```

Validate model metadata without loading llama.cpp:

```powershell
.\build\Debug\prism-probe.exe --mode 1gb-safe-cpu --model tiny.gguf --sidecar tiny.gguf.prism.json
```

The sidecar skeleton validates path normalization, file size limits, schema
version, `model_sha256`, and malformed sidecar rejection before any model
runtime integration exists.

Fail-closed cap paths can be tested without large allocations:

```powershell
.\build\Debug\prism-probe.exe --mode 1gb-safe-cpu --simulate-allocator-peak-bytes 1073741825
```

## Research and Roadmap

Current research direction and post-Phase-0 implementation plans are tracked in:

- `docs/research-roadmap-constrained-llm-inference.md`
- `docs/validation-matrix.md`
- `docs/claim-taxonomy.md`
- `docs/host-memory-and-io-telemetry.md`
- `docs/windows-wddm-telemetry-policy.md`
- `docs/kv-cache-and-compression-research.md`
- `docs/phase1-implementation-plan.md`
- `docs/phase2-implementation-plan.md`
- `docs/phase3-implementation-plan.md`
- `docs/phase4-implementation-plan.md`
- `docs/phase4-90b-validation-policy.md`
- `docs/phase5-compute-kernel-research-plan.md`
- `docs/phase6-implementation-plan.md`
- `docs/phase6-compression-architecture.md`
- `docs/phase6-evidence.md`
- `docs/kernel-benchmark-methodology.md`
- `docs/implementation-plan-phase1-to-phase4.md`

The current roadmap caps GPU hard-limit validation at 16 GiB. The 90B hybrid
profile is simulated/offline only until validated benchmark evidence exists.
The first custom CUDA kernel target is the Phase 5 gated q4 decode-GEMV
prototype. Broader fused dequantization, Tensor Core paths, IO-aware attention,
MLA latent KV, low-rank compression, structured sparsity, and MoE runtime support
remain gated until same-cell benchmark evidence and cap accounting are retained.
Phase 6 is planned as the manifest-backed 9B evidence construction stage for
that CUDA kernel lane plus exact-cell compression evidence. It still makes no
9B constrained-inference, Tensor Core, deployable-profile, or bucket-wide
`>5B-10B` claim until retained artifacts pass the documented gates.
Historical council notes live under `docs/archive/`.
