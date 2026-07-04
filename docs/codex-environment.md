# Codex Environment

Use these commands when configuring this repository in Codex environment setup.

## Setup

Default setup and verification:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-setup.ps1
```

CUDA-capable setup and verification:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-setup.ps1 -WithCuda
```

Install missing helper tools when needed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-setup.ps1 -InstallMissing
```

The setup script checks expected tools, validates Visual Studio Build Tools, runs workflow lint, configures the default build, builds Debug, and runs CTest. With `-WithCuda`, it also validates CUDA tools and the CUDA probe build.

## Verification Only

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1
```

CUDA verification:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1 -WithCuda
```

## Cleanup

Preview cleanup:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-clean.ps1 -WhatIf
```

Remove generated build directories and probe artifacts:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-clean.ps1
```

Also remove local Visual Studio state:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-clean.ps1 -All
```

## GitHub Workflows

Repository workflows remain under `.github/workflows/`:

- `cmake-ci.yml`: hosted CMake and CTest validation.
- `cuda-probe-self-hosted.yml`: self-hosted Windows/NVIDIA/CUDA probe validation.
- `project-automation.yml`: GitHub Project sync when `PROJECT_TOKEN` exists.
