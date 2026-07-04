# Codex Environment

Use these commands when configuring this repository in Codex environment setup.

## Setup

Default setup and verification when the Codex environment root is the repository root:

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

If the Codex environment root is `D:\Research` rather than `D:\Research\prisminfer`, use this Windows setup script instead:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command "Set-Location -LiteralPath $env:CODEX_WORKTREE_PATH; if (Test-Path 'scripts\dev-setup.ps1') { & 'scripts\dev-setup.ps1' -WithCuda -InstallMissing } elseif (Test-Path 'prisminfer\scripts\dev-setup.ps1') { Set-Location 'prisminfer'; & 'scripts\dev-setup.ps1' -WithCuda -InstallMissing } else { Write-Host 'No PrismInfer setup script found in this worktree; skipping setup.' }"
```

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
- `cuda-probe-self-hosted.yml`: self-hosted Windows/NVIDIA/CUDA probe validation.
- `project-automation.yml`: GitHub Project sync when `PROJECT_TOKEN` exists.
