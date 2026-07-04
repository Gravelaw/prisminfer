# AGENTS.md

## Purpose

This file contains PrismInfer-specific guidance for agents working in this repository.

General SDLC behavior is governed outside this repo. Keep this file focused on project-specific C/C++ development, build/test expectations, GitHub workflow awareness, and PrismInfer architectural constraints.

## Project Direction

PrismInfer is a C/C++ inference-governor and telemetry project for constrained-memory LLM inference.

The repository is expected to evolve through multiple phases.

Core direction:

- Prefer compatibility with llama.cpp, GGML, and GGUF ecosystems over a clean-sheet runtime.
- Treat memory-cap evidence as a first-class product requirement.
- Preserve fail-closed behavior when telemetry is missing, contradictory, or incomplete.
- Keep CUDA support optional at build time unless the task explicitly targets CUDA.
- Only add custom kernels, runtime rewrites, or model-loading integrations when the scoped issue requires them.

## Tracking

Primary repo:

- GitHub: `Gravelaw/prisminfer`
- Local path: `D:\Research\prisminfer`
- Default branch: `main`

GitHub is the source of truth for scoped work:

- Use GitHub Issues for work items.
- Use GitHub Projects for status, priority, risk, gates, and dates.
- Use milestones for phase boundaries.
- Link PRs to issues with `Closes #<issue>` or `Refs #<issue>`.

Normal development should happen through issue branches and PRs.

## Environment

Codex local environment commands are documented in:

- `docs/codex-environment.md`

The Codex environment may be rooted at `D:\Research` while this repo is under `D:\Research\prisminfer`, so use the robust commands from that document when configuring Codex setup, cleanup, and actions.

## Build and Test

Primary Windows development path:

```powershell
cmake -S . -B build
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

CUDA-enabled probe path:

```powershell
cmake -S . -B build-cuda -DPRISMINFER_ENABLE_CUDA_PROBE=ON
cmake --build build-cuda --config Debug --parallel
ctest --test-dir build-cuda -C Debug --output-on-failure
```

Preferred full verification:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1
```

CUDA verification:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1 -WithCuda
```

For workflow changes, run `actionlint`; `scripts\verify.ps1` already does this.

## GitHub Actions

Current workflows:

- `.github/workflows/cmake-ci.yml`
  - Hosted CMake/CTest CI.
  - Runs Windows Debug, Windows Release, and Ubuntu Debug.

- `.github/workflows/cuda-probe-self-hosted.yml`
  - Self-hosted Windows/NVIDIA/CUDA probe validation.
  - Requires runner labels: `self-hosted`, `Windows`, `NVIDIA`, `CUDA`.
  - Do not assume this runs on GitHub-hosted runners.

- `.github/workflows/project-automation.yml`
  - Syncs issues and PRs to the GitHub Project when `PROJECT_TOKEN` is configured.
  - If the secret is missing, it exits successfully without mutation.

Branch protection may not be available on the private repo without the required GitHub plan. Do not assume GitHub enforces PR-only merges unless verified.

## Engineering Notes

Use C++20 through CMake.

Prefer:

- Small, focused translation units.
- Clear boundaries between config, telemetry, governor, benchmark, tools, and tests.
- Standard library facilities before adding dependencies.
- Deterministic tests that do not depend on large real allocations.
- Explicit error reasons for fail-closed paths.

Avoid:

- Hidden global state for telemetry or cap decisions.
- Weakening cap checks to make tests pass.
- Treating device memory delta alone as hard-cap proof.
- Adding dependencies without a clear reason and build impact.
- Committing generated binaries, build directories, probe JSONL, or manifests unless intentionally added as fixtures.

## Probe Behavior

For probe changes, run the relevant `prism-probe` smoke path and validate emitted JSONL with `prism-validate-lifecycle`.

Forced breach paths should fail closed with specific reasons. Do not allocate excessive real GPU memory just to test breach behavior.

## Important Files

Start with these when changing related areas:

- `README.md`
- `CMakeLists.txt`
- `docs/cap-semantics.md`
- `docs/runtime-state-machine.md`
- `docs/risk-register.md`
- `docs/ci-and-project-workflows.md`
- `docs/codex-environment.md`
- `schemas/telemetry.schema.json`
- `schemas/benchmark_manifest.schema.json`
- `schemas/prism_sidecar.schema.json`

Main code areas:

- `include/prisminfer/`
- `src/config.cpp`
- `src/governor/`
- `src/telemetry/`
- `src/benchmark/`
- `tools/prism-probe/`
- `tools/prism-validate-lifecycle/`
- `tests/unit/`

## Before Finishing

Run the repo verification script before handing work back:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1
```

For CUDA-touching changes, also run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1 -WithCuda
```

For docs-only changes, a lighter verification is acceptable, but report what was run and what was skipped.
