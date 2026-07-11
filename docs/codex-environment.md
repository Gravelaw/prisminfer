# Codex Environment

Use these commands when configuring this repository in Codex environment setup.

## Safety-First Preflight

Run dependency discovery without building, installing packages, or touching the
GPU first:

```powershell
git status --short --untracked-files=all
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-setup.ps1 -SkipBuild
```

For a CUDA-capable task, the dependency-only extension is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-setup.ps1 -WithCuda -SkipBuild
nvidia-smi --query-gpu=name,driver_version,pstate,temperature.gpu,power.draw,memory.total,memory.used,memory.free --format=csv
```

`-InstallMissing` changes machine state. Use it only after the user explicitly
authorizes the exact installation scope. Never use environment setup to change
GPU clocks/power/fans, Windows TDR, the pagefile, Device Guard, drivers, CUDA,
firmware, or BIOS settings. Follow the admission and stop-work rules in
`AGENTS.md` before any hardware workload.

## Validated Reference Host (2026-07-11)

This is a point-in-time dependency snapshot, not a substitute for per-session
preflight:

| Area | Observed state | Readiness |
|---|---|---|
| OS/CPU/RAM | Windows 11 25H2 build 26200.8655; Intel Core Ultra 9 285H; 31.43 GiB RAM | CPU development ready; preserve physical-RAM and commit headroom |
| GPU | RTX 5080 Laptop GPU, compute capability `sm_120`, WDDM, 16,303 MiB physical VRAM | Tiny attended CUDA fixtures only; 16 GiB is a claim ceiling, not an allocation target |
| NVIDIA stack | Driver 610.62; CUDA toolkit/compiler 13.3.73 | Dependency preflight passes |
| C++ build | VS Build Tools 2026 18.7.3, MSVC 19.51.36248, CMake/CTest 4.3.3, Ninja 1.13.2 | Required build lane ready |
| Analysis/profiling | LLVM/clang/clang-tidy/clang-format 22.1.8; Compute Sanitizer and Nsight Compute 2026.2.1 | Core analysis tools ready; Nsight Systems is installed but not on `PATH` |
| Repository tooling | Git 2.54, GitHub CLI 2.93, actionlint 1.7.12, jq 1.8.2 | Required workflow lane ready |
| Python | 3.14 default; 3.13 environment carries the current scientific stack | Pin the interpreter per script instead of assuming the default has dependencies |
| Host security | Device Guard/code integrity enforced; no explicit TDR override was found | Never weaken policy; harmlessly launch new executables before GPU access |

Optional tools not currently installed include cppcheck, Include What You Use,
CodeQL CLI, PSScriptAnalyzer, CMake lint, ccache, and Doxygen. They are not
blockers for the CPU baseline. The blockers for sustained CUDA/model work are
runtime admission, process containment, a hardware watchdog/cancellation path,
checked-arithmetic hardening, and approved model/prompt hashes—not additional
package installation.

## Setup

Default setup and verification when the Codex environment root is the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-setup.ps1
```

CUDA-capable setup and verification:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-setup.ps1 -WithCuda
```

Install missing helper tools only after explicit authorization:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-setup.ps1 -InstallMissing
```

The setup script checks expected tools, validates Visual Studio Build Tools, runs workflow lint, configures the default build, builds Debug, and runs CTest. With `-WithCuda`, it also validates CUDA tools and the CUDA probe build.

If the Codex environment root is `D:\Research` rather than `D:\Research\prisminfer`, use this dependency-only Windows preflight instead:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command "Set-Location -LiteralPath $env:CODEX_WORKTREE_PATH; if (Test-Path 'scripts\dev-setup.ps1') { & 'scripts\dev-setup.ps1' -WithCuda -SkipBuild } elseif (Test-Path 'prisminfer\scripts\dev-setup.ps1') { Set-Location 'prisminfer'; & 'scripts\dev-setup.ps1' -WithCuda -SkipBuild } else { Write-Host 'No PrismInfer setup script found in this worktree; skipping setup.' }"
```

## Verification Only

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1
```

Attended CUDA probe verification, only after `AGENTS.md` preflight and stop
conditions pass:

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

If the Codex environment root is `D:\Research` rather than `D:\Research\prisminfer`, use this Windows cleanup script instead:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command "Set-Location -LiteralPath $env:CODEX_WORKTREE_PATH; if (Test-Path 'scripts\dev-clean.ps1') { & 'scripts\dev-clean.ps1' } elseif (Test-Path 'prisminfer\scripts\dev-clean.ps1') { Set-Location 'prisminfer'; & 'scripts\dev-clean.ps1' } else { Write-Host 'No PrismInfer cleanup script found in this worktree; skipping cleanup.' }"
```

## Actions

If the Codex environment root is `D:\Research`, use these action commands.

Verify:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command "Set-Location -LiteralPath $env:CODEX_WORKTREE_PATH; if (Test-Path 'scripts\verify.ps1') { & 'scripts\verify.ps1' } elseif (Test-Path 'prisminfer\scripts\verify.ps1') { Set-Location 'prisminfer'; & 'scripts\verify.ps1' } else { throw 'No PrismInfer verify script found.' }"
```

Verify CUDA:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command "Set-Location -LiteralPath $env:CODEX_WORKTREE_PATH; if (Test-Path 'scripts\verify.ps1') { & 'scripts\verify.ps1' -WithCuda } elseif (Test-Path 'prisminfer\scripts\verify.ps1') { Set-Location 'prisminfer'; & 'scripts\verify.ps1' -WithCuda } else { throw 'No PrismInfer verify script found.' }"
```

Clean Preview:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command "Set-Location -LiteralPath $env:CODEX_WORKTREE_PATH; if (Test-Path 'scripts\dev-clean.ps1') { & 'scripts\dev-clean.ps1' -WhatIf } elseif (Test-Path 'prisminfer\scripts\dev-clean.ps1') { Set-Location 'prisminfer'; & 'scripts\dev-clean.ps1' -WhatIf } else { Write-Host 'No PrismInfer cleanup script found in this worktree; skipping cleanup preview.' }"
```

Clean:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command "Set-Location -LiteralPath $env:CODEX_WORKTREE_PATH; if (Test-Path 'scripts\dev-clean.ps1') { & 'scripts\dev-clean.ps1' } elseif (Test-Path 'prisminfer\scripts\dev-clean.ps1') { Set-Location 'prisminfer'; & 'scripts\dev-clean.ps1' } else { Write-Host 'No PrismInfer cleanup script found in this worktree; skipping cleanup.' }"
```

## GitHub Workflows

Repository workflows remain under `.github/workflows/`:

- `cmake-ci.yml`: hosted CMake and CTest validation.
- `cuda-probe-self-hosted.yml`: manual-only self-hosted Windows/NVIDIA/CUDA probe validation.
- `cuda-kernel-self-hosted.yml`: manual-only tiny synthetic CUDA correctness validation.
- `offload-profitability-self-hosted.yml`: manual-only transfer-inclusive profitability validation.
- `project-automation.yml`: GitHub Project sync when `PROJECT_TOKEN` exists.
