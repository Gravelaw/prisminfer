# PrismInfer

PrismInfer is a low-VRAM LLM inference governor, telemetry harness, and
research scaffold for constrained GPU memory claims.

The canonical program order, clearances, dependency matrix and live tracker
contract are in [`Plan.md`](Plan.md). Detailed phase and adaptive-runtime
documents are subordinate to that plan.

The v0 direction is intentionally conservative:

- use llama.cpp/GGML/GGUF compatibility rather than a clean-sheet runtime,
- start with `1gb-safe-cpu`,
- allow GPU only through `1gb-safe-gpu-probed`,
- fail closed when hard-cap telemetry is missing or contradictory,
- include one gated CUDA kernel prototype in Phase 5 after the benchmark,
  schema, runtime evidence, 9B validation-cell, and allocation-reconciliation
  gates pass.

## Development Safety

Read `AGENTS.md` and `docs/codex-environment.md` before building or running the
project. Start with the dependency-only preflight; CUDA is never an implicit
setup step. Full-model, sustained CUDA, and 9B evidence runs remain prohibited
until live resource admission, bounded process containment, checked arithmetic,
and the hardware watchdog/cancellation path are implemented and verified.
Host RAM admission is workload-relative: there is no fixed 24 GiB-free gate.
CPU-safe development may use the non-promotable T-101 development lane, while
promotable evidence requires a fresh stricter evidence-lane decision.

## Build

On Windows, use the guarded local verification path:

```powershell
.\scripts\verify.ps1
```

The default conservatively estimates build parallelism from a fresh physical
and commit snapshot, the T-101 development reserves, and 2 GiB per job. This is
a non-promotable preflight, not a live process-tree memory cap. `-BuildJobs
1..8` is an upper cap only; it cannot bypass the memory-derived bound.

On Linux or macOS, `verify.ps1` is not the local entry point. Use a bounded
CPU-only configure/build/test sequence; this does not create a promotable host
admission receipt:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 1
ctest --test-dir build --timeout 60 --output-on-failure
```

CUDA probing is optional and uses a separate build directory:

```powershell
cmake -S . -B build-cuda -DPRISMINFER_ENABLE_CUDA_PROBE=ON
cmake --build build-cuda --config Debug --parallel 1
ctest --test-dir build-cuda -C Debug --timeout 60 --output-on-failure
```

CUDA kernel prototype builds are gated separately from probe support. On
Windows, the checked-in CMake preset targets Visual Studio 2026 and an sm_120
GPU:

```powershell
cmake --preset vs2026-cuda-sm120
cmake --build --preset vs2026-cuda-sm120 --parallel 1
ctest --test-dir build/vs2026-cuda-sm120 -C Debug -L cuda_kernel --timeout 60 --output-on-failure
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

- `Plan.md`
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
- `docs/adaptive-runtime/README.md`

The current roadmap caps GPU hard-limit validation at 16 GiB. The 90B hybrid
profile is simulated/offline only until validated benchmark evidence exists.
The active program first implements the fail-closed supervisor, staged
admission, exact per-tensor GGML quant truth, quality fixtures and supervised
same-cell evidence. The current q4 CUDA path is a tiny synthetic correctness
fixture. A custom-kernel or KV-compression win is optional and cannot block the
static controller. No 8B/9B constrained-inference, Tensor Core,
deployable-profile, or bucket-wide claim is made until the exact retained gates
pass.
The detailed adaptive-runtime program is documented under
`docs/adaptive-runtime/`. Its first proof is deliberately narrower than a
general dynamic runtime: a secure, actuator-bound static controller must safely
select and replay an acknowledged plan on the exact admitted Llama 3.1 8B
foundation cell while preserving memory, semantics, quality, and tail-latency
gates. Safe selection of the upstream winner proves the controller, but is not
a speedup claim. Ornith is retained as a separate hybrid-architecture stress
cell, not used to generalize ordinary KV-cache conclusions. Larger 30B, 70B,
and 90B work remains gated by explicit
host-memory, storage, transfer-bandwidth, and latency admission calculations.
Curated source-controlled history lives under `docs/archive/`. Superseded raw
working material may be retained locally under the ignored `.local-archive/`,
but no repository claim or instruction may depend on that local directory.
