# CI and Project Workflows

Phase 0 uses two classes of GitHub automation.

## Required CMake CI

`.github/workflows/cmake-ci.yml` runs on every push and pull request to `main`.

It validates:

- Windows Debug with MSVC.
- Windows Release with MSVC.
- Linux Debug with Ninja.
- `ctest --output-on-failure` for every build.

This workflow does not enable `PRISMINFER_ENABLE_CUDA_PROBE` because GitHub-hosted runners do not provide an NVIDIA GPU suitable for hard-cap evidence.

## CUDA Probe CI

`.github/workflows/cuda-probe-self-hosted.yml` runs on a self-hosted Windows runner labeled:

- `self-hosted`
- `Windows`
- `NVIDIA`
- `CUDA`

It validates:

- CUDA probe configure with `-DPRISMINFER_ENABLE_CUDA_PROBE=ON`.
- Debug build and CTest.
- CPU probe smoke.
- GPU-probed smoke.
- Forced allocator breach smoke.
- Lifecycle validation for emitted JSONL.
- Artifact upload for JSONL and manifest evidence.

This workflow is intentionally separate from required hosted CI. It should become a Phase 0 exit gate only after the self-hosted runner is stable.

## Project Automation

`.github/workflows/project-automation.yml` adds newly opened or reopened issues and pull requests to the user project:

- Owner: `Gravelaw`
- Project: `PrismInfer Phase 0`
- Project number: `2`

Because this is a user-level GitHub Project, the default `GITHUB_TOKEN` cannot reliably mutate the project. Add a repository secret named `PROJECT_TOKEN` with project write access before enabling this automation as a required workflow.

Default field behavior:

- `Status`: `Todo`
- `Phase Status`: `Backlog`

Detailed planning fields such as `Slice`, `Risk`, `Gate`, `Priority`, `Start Date`, and `Target Date` should still be maintained deliberately for Phase 0 planning items.

