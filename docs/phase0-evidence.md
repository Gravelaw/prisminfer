# Phase 0 Evidence Record

This file records the minimum evidence needed before Phase 0 exit can be
claimed. Evidence is valid only when the corresponding command output,
manifest, telemetry JSONL, or GitHub Actions run is retained or linked from the
Phase 0 issue/PR.

## Current Dependency Pins

| Item | Current record | Notes |
|---|---|---|
| PrismInfer commit | `3d2b830` baseline before this evidence hardening slice | Replace with the merge commit for Phase 0 exit. |
| CMake | Recorded in every benchmark manifest as `cmake_version` | Generated from `PRISMINFER_CMAKE_VERSION`. |
| Build config | Recorded in every benchmark manifest as `build_config` | Multi-config generators report values such as `Debug`. |
| C++ compiler | Recorded in every benchmark manifest as `compiler` | MSVC is the primary Windows compiler. |
| CUDA probe option | Recorded in every benchmark manifest as `cuda_probe_compiled` | `false` for default builds, `true` for CUDA probe builds. |
| CUDA runtime/driver | Recorded in every CUDA-probed manifest | Values are `0` when CUDA probe support is not active. |
| llama.cpp/GGML/GGUF | Not pinned in Phase 0 | Add exact upstream commit and artifact hashes before Phase 1 backend work. |

## Source And Artifact Index

| Source or artifact | Required evidence |
|---|---|
| Hosted CMake CI | Successful GitHub Actions run for Windows Debug, Windows Release, and Ubuntu Debug. |
| Self-hosted CUDA probe | Successful `CUDA Probe Self-Hosted` run with uploaded JSONL and manifest artifacts. |
| Local default verification | `scripts\verify.ps1` output showing workflow lint, Debug build, CTest, CPU smoke, forced breach matrix, and lifecycle validation. |
| Local CUDA verification | `scripts\verify.ps1 -WithCuda` output showing default verification plus CUDA build, CUDA CTest, GPU-probed smoke, and lifecycle validation. |
| Benchmark manifests | Manifest files must include run id, build config, compiler, CMake version, CUDA option, OS/GPU info, telemetry path, status, and failure reason. |
| Telemetry JSONL | JSONL must pass `prism-validate-lifecycle` for CPU-safe, GPU-probed, and forced-failure paths. |

## Hardware Evidence Classes

| Class | Definition | Phase 0 status |
|---|---|---|
| Class A CPU-only | Default build with CPU-safe probe and forced breach matrix. | Required for every PR touching runtime, telemetry, or validation. |
| Class B local CUDA | Windows NVIDIA target where `1gb-safe-gpu-probed` succeeds locally. | Current machine is the first evidence target. |
| Class C self-hosted CUDA CI | GitHub self-hosted runner labeled `self-hosted`, `Windows`, `NVIDIA`, `CUDA`. | Required before Phase 0 exit gate is closed. |

## CUDA Toolchain Note

The CUDA probe build may fall back to CUDAToolkit runtime linking when the CMake
CUDA language compiler is not available for the active generator. That fallback
is acceptable for Phase 0 probe evidence only when the manifest, telemetry, and
CUDA smoke all pass. Issue #2 remains the place to decide whether a full CUDA
language profile is required before Phase 0 exit.
