# PrismInfer

PrismInfer is a Phase 0 scaffold for a low-VRAM LLM inference governor and telemetry harness.

The v0 direction is intentionally conservative:

- use llama.cpp/GGML/GGUF compatibility rather than a clean-sheet runtime,
- start with `1gb-safe-cpu`,
- allow GPU only through `1gb-safe-gpu-probed`,
- fail closed when hard-cap telemetry is missing or contradictory,
- postpone custom CUDA kernels until after Phase 0 and Phase 1 pass.

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
