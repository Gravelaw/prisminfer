# Phase 0 Evidence Record

This file records the minimum evidence needed before Phase 0 exit can be
claimed. Evidence is valid only when the corresponding command output,
manifest, telemetry JSONL, or GitHub Actions run is retained or linked from the
Phase 0 issue/PR.

## Current Dependency Pins

| Item | Current record | Notes |
|---|---|---|
| PrismInfer implementation baseline | `dff44d3f69044a9e32c7d801e31f00317b57f512` | Source commit under Phase 0 exit audit before this docs-only evidence PR. |
| CMake | `4.3.3` | Recorded in CUDA evidence manifests as `cmake_version`. |
| Build config | Recorded in every benchmark manifest as `build_config` | Multi-config generators report values such as `Debug`. |
| C++ compiler | `MSVC 1944` for Windows CUDA evidence | Recorded in CUDA evidence manifests as `compiler`; Linux hosted CI validates GCC-compatible build separately. |
| CUDA probe option | Recorded in every benchmark manifest as `cuda_probe_compiled` | `false` for default builds, `true` for CUDA probe builds. |
| CUDA Toolkit/runtime/driver | Toolkit `13.3.33`, runtime `13030`, driver `13030` | Toolkit version is from self-hosted CUDA configure logs; runtime/driver are from `ci-gpu-smoke-manifest.json`. |
| llama.cpp/GGML/GGUF | Not pinned in Phase 0 | Add exact upstream commit and artifact hashes before Phase 1 backend work. |

## Source And Artifact Index

| Source or artifact | Required evidence |
|---|---|
| Hosted CMake CI | Successful GitHub Actions run for Windows Debug, Windows Release, and Ubuntu Debug: https://github.com/Gravelaw/prisminfer/actions/runs/28700587279 |
| Self-hosted CUDA probe | Successful `CUDA Probe Self-Hosted` run with uploaded JSONL and manifest artifacts: https://github.com/Gravelaw/prisminfer/actions/runs/28700552514 |
| Local default verification | `scripts\verify.ps1` output showing workflow lint, Debug build, CTest, CPU smoke, forced breach matrix, and lifecycle validation. |
| Local CUDA verification | `scripts\verify.ps1 -WithCuda` output showing default verification plus CUDA build, CUDA CTest, GPU-probed smoke, and lifecycle validation. |
| Benchmark manifests | Manifest files must include run id, build config, compiler, CMake version, CUDA option, OS/GPU info, telemetry path, status, and failure reason. |
| Telemetry JSONL | JSONL must pass `prism-validate-lifecycle` for CPU-safe, GPU-probed, and forced-failure paths. |

## Pinned CUDA Evidence Artifacts

Access date: 2026-07-04.

| Artifact | SHA-256 |
|---|---|
| `ci-cpu-smoke.jsonl` | `B52317CDBCCC52B3A575FC754252EE92FB18504970BD716420FB48D0C7E4FC1F` |
| `ci-cpu-smoke-manifest.json` | `AB69AD630489A657DC778E20E1F15887F69180E4B819B57960B9F5688D0E6367` |
| `ci-gpu-smoke.jsonl` | `9BE85FE4B163AE698CCC7AD054611BB2B208D7BAB379D44E8232B1CCCDC82C84` |
| `ci-gpu-smoke-manifest.json` | `40B66A7F61A7D0D4024568BFC61B609EE003E3A05C46C6E3466016D3DD145572` |
| `ci-forced-allocator.jsonl` | `AC2A2308CC87016FC3B0C8224F78B32DC2A880638E42BDC8628AAE4AD55F9965` |
| `ci-forced-allocator-manifest.json` | `E48A7A1B903D60B45C68695708FAD52252C4B9A010AC99145A9EF02775349170` |

## Hardware Evidence Classes

| Class | Definition | Phase 0 status |
|---|---|---|
| Class A CPU-only | Default build with CPU-safe probe and forced breach matrix. | Proven by local `scripts\verify.ps1`, local `scripts\verify.ps1 -WithCuda`, and hosted CMake CI run `28700587279`. |
| Class B local CUDA | Windows NVIDIA target where `1gb-safe-gpu-probed` succeeds locally. | Proven on this workstation by `scripts\verify.ps1 -WithCuda` against CUDA Toolkit 13.3. |
| Class C self-hosted CUDA CI | GitHub self-hosted runner labeled `self-hosted`, `Windows`, `NVIDIA`, `CUDA`. | Proven by run `28700552514` on `NVIDIA GeForce RTX 5080 Laptop GPU`. |

## CUDA Toolchain Note

The CUDA probe build may fall back to CUDAToolkit runtime linking when the CMake
CUDA language compiler is not available for the active generator. That fallback
is acceptable for Phase 0 probe evidence only when the manifest, telemetry, and
CUDA smoke all pass. Issue #2 remains the place to decide whether a full CUDA
language profile is required before Phase 0 exit.
