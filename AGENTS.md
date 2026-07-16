# AGENTS.md

## Scope

- The PrismInfer repository root is `D:\Research\prisminfer`. All PrismInfer code, documentation, configuration, tests, generated planning artifacts, and Git operations must be scoped beneath that root.
- A session opened at `D:\Research` is one directory above the repository. Enter `D:\Research\prisminfer` before inspecting or changing the project; do not create or edit PrismInfer files in `D:\Research` or a sibling checkout.
- This `AGENTS.md` is authoritative for repository-operation and hardware-safety invariants. `Plan.md` is the canonical program-control document and controls program thesis, scope, dependency order, clearances, live status, and phase exit. [`docs/adaptive-runtime-v2/README.md`](docs/adaptive-runtime-v2/README.md) maps the detailed V2 owner contracts. Phase documents, issues, PRs, and Project fields are operational/detail sources and cannot override either top-level authority.
- PrismInfer is a C++20 CMake project for low-VRAM inference governance, telemetry, and evidence; keep CUDA, llama.cpp, GGML, and GGUF work compatible with that ecosystem rather than inventing a new runtime.
- Memory-cap evidence is product behavior: missing, contradictory, or incomplete telemetry must fail closed, not warn-and-pass.
- Do not promote 9B, Tensor Core, deployable-profile, speedup, or bucket-wide claims unless retained manifests/artifacts satisfy the relevant phase docs.
- CUDA must remain optional unless the task explicitly targets CUDA. `PRISMINFER_ENABLE_CUDA_PROBE` and `PRISMINFER_ENABLE_CUDA_KERNELS` are separate lanes.

## Safety Authority and Stop-Work Rules

- Correctness, machine stability, data integrity, and retained evidence take precedence over throughput. A slower safe result or an evidence-backed rejection is preferable to an unstable run.
- Missing, stale, contradictory, or unavailable guard telemetry; unchecked arithmetic; a rejected reservation; timeout; thermal or power-brake slowdown; TDR/device reset; device-lost error; unexplained process death; or cap breach stops the run and blocks promotion. Do not automatically retry a hardware-fatal error in the same process.
- Never change GPU clocks, voltage, power limits, thermal limits, fan policy, persistence mode, Windows TDR registry values, pagefile policy, firmware, BIOS, driver policy, or OS security controls unless the user explicitly requests that exact state change. Never disable Device Guard or code-integrity enforcement to make an unsigned test executable run.
- A benchmark, calibration, full-model load, or stress run must never be the first execution of new allocator, parser, process-launch, or kernel code. Start with static analysis, CPU/reference tests, and tiny deterministic fixtures.
- Do not run unattended, long-duration, full-model, 9B, 30B, 70B, or 90B hardware workloads until process containment, admission-before-workload, a hardware watchdog, bounded cancellation, and abort telemetry are implemented and verified.
- The 16 GiB value is a product/claim ceiling, not an allocation target. The current reference GPU reports 16,303 MiB, which is smaller than 16 GiB. Effective admission must use the minimum of the policy cap, current WDDM/DXGI local budget, and current CUDA availability, then subtract a documented nonzero reserve.
- CUDA context creation is itself a metered allocation step. Perform a read-only device/budget preflight first, ledger the context, and stop before model/workload allocation if admission rejects or evidence cannot be reconciled.
- Do not use a real allocation to prove a breach. Use the existing simulation/fault-injection flags.

## Agent Model and Review Assurance

- `gpt-5.6-terra` with medium reasoning is an approved default for bounded, issue-scoped documentation, build/schema/CLI work, deterministic CPU references and fixtures, ordinary telemetry, calibration storage, and planner code. It is a productivity preset, never a correctness, evidence, safety, or hardware-clearance class.
- Terra-medium may draft but cannot solely approve or promote checked admission/allocation arithmetic, untrusted parsers or GGUF decoders, native process/Job containment, supervisor/watchdog/recovery, WDDM/DXGI evidence, CUDA/GGML/provider code, or recovery state machines.
- Safety-critical changes require an independent diff review by a non-authoring agent run, preferably a higher-assurance reasoning/pro or coding-specialized preset, or a qualified human. The reviewer must inspect the patch and retained executable/fault evidence; the author cannot mark the issue complete, and model opinion alone is insufficient.
- A persistent program-level goal may coordinate PrismInfer, but it must keep only one dependency-safe integration packet active at a time. It sequences bounded issue checkpoints and stops at packet review, clearance, and authorization gates; it is never blanket implementation or hardware authorization.
- Within a packet, work one GitHub issue or explicitly bounded sub-slice at a time. Give every checkpoint its own issue, commit or exact tree receipt, allowed files, prohibited actions, dependencies, tests, and claim boundary. Never ask a model to implement the program as one undifferentiated change.
- No agent may weaken `AGENTS.md`, `Plan.md`, an applicable V2 owner/phase contract, thresholds, tests, or evidence to approve its own work. Governing-contract changes must be explicit and independently reviewed; real-hardware promotion still requires the applicable clearance and human authorization.
- If model names change, preserve these implementation and independent-review roles with the nearest equivalent presets.

## Integration Trains and Review Inheritance

- `Plan.md` defines the permitted integration packets. The default is one branch and one integration PR per packet, with predecessor-safe issue checkpoints committed sequentially. Do not create speculative future or stacked PRs merely to mirror every issue.
- Use a child PR only for a concurrent author, a separately trusted review boundary, or a diff that becomes too large or heterogeneous to review safely. Its reviewed exact head/tree, tests, risk class, and reviewer become a receipt for the parent packet.
- At the start of a persistent task, perform one complete repository/GitHub recovery. At an issue checkpoint, re-read only changed governing contracts and inspect the current root, branch, status, base/head, checkpoint diff, dependencies, and targeted tests. Repeat complete recovery when `main`, `AGENTS.md`, `Plan.md`, the applicable sole-owner V2 contract, the packet base, or unexplained external state changes.
- A review receipt must record the base SHA, reviewed head and tree SHA, covered paths/issues, risk tier, validation commands/results, author/reviewer provenance, and hardware/evidence status. Parent and phase reviews may inherit an exact receipt after verifying ancestry and an unchanged tree.
- An inheriting review examines only unreviewed commits, integration glue, conflict resolution, changed governing contracts, and evidence/claim deltas. Changing a reviewed path invalidates the affected receipt, not unrelated reviewed history. Squash/rebase requires tree or stable-patch equivalence, not a verbal assertion.
- Run focused checks at issue checkpoints and the full applicable CPU/hosted lane once at the final packet head. Report routine mechanical checks such as `git diff --check` as concise pass/fail results. A failing focused check, uncertain dependency, or material scope expansion stops the packet.
- Safety-critical code still receives independent exact-head review before merge. Repository-wide or deep security/evidence review occurs at the phase exits declared by `Plan.md`, not automatically for unchanged or low-risk checkpoints.
- A packet goal may update branches, PRs, issues, and Project #2 only when the user's goal explicitly authorizes those GitHub mutations. It never inherits authorization for self-hosted workflows, CUDA/model execution, downloads, calibration, benchmarks, or sustained hardware work.

## Mandatory Session and Hardware Preflight

Run the first group before development; run the CUDA group only before CUDA commands:

```powershell
# Repository and dependencies
Set-Location D:\Research\prisminfer
$repoRoot = (git rev-parse --show-toplevel).Trim().Replace('/', '\')
if ($repoRoot -ne 'D:\Research\prisminfer') { throw "Wrong repository root: $repoRoot" }
git status --short --untracked-files=all
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-setup.ps1 -SkipBuild
if (Test-Path -LiteralPath CMakePresets.json) { cmake --list-presets }

# Additional CUDA preflight
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-setup.ps1 -WithCuda -SkipBuild
nvidia-smi --query-gpu=name,driver_version,pstate,temperature.gpu,power.draw,memory.total,memory.used,memory.free,pcie.link.gen.current,pcie.link.width.current --format=csv
nvidia-smi -q -d TEMPERATURE,POWER,PERFORMANCE
```

- `-InstallMissing` changes machine state. Use it only with explicit user authorization; do not silently install or upgrade compilers, drivers, CUDA, analyzers, package managers, or plugins.
- Record OS, CPU/RAM, free physical RAM and commit headroom, disk free space, GPU/driver/CUDA, WDDM mode and budget, power source, temperature/throttle state, CMake/compiler versions, exact CUDA architecture, dependency hashes, and repository commit in the run manifest.
- Require AC power, unobstructed cooling, no competing GPU workload, a stable device baseline, sufficient physical-RAM/commit/disk reserve, and no active thermal or hardware power-brake slowdown. Host sufficiency is the T-101 lane reserve plus the exact workload's incremental resident and commit peaks; never impose a fixed free-RAM prerequisite such as 24 GiB. If a required sensor is unavailable, long/unattended runs fail closed.
- Read TDR values only. Keep Windows defaults; Microsoft documents a default two-second TDR timeout. New dispatches must be tiled so measured p99 dispatch time is below the project safety gate of 250 ms before scale-up. This is a PrismInfer engineering margin, not a Microsoft guarantee.
- Record the device-specific temperature stop threshold below the reported slowdown/target limit in the active risk and validation contracts. [`docs/adaptive-runtime-v2/evidence-thresholds-and-security.md`](docs/adaptive-runtime-v2/evidence-thresholds-and-security.md) owns the active threshold registry. Until the `Plan.md` hardware-supervisor gate closes, only short, attended, tiny-fixture CUDA tests are allowed.
- Harmlessly launch every newly built executable (`--help` or a tiny CPU fixture) before a hardware run. If Windows Code Integrity blocks it, stop and use an approved signing/trusted-runner path; do not weaken host security.
- Verify the exact model/tokenizer/quant/config hashes before model work. Absence of a local approved 9B artifact means 9B execution is not ready.

## Hardware Experiment Ladder

Advance one rung at a time and retain the result of each rung:

1. Static analysis, checked-arithmetic/unit tests, CPU reference, and simulated breach paths.
2. Tiny deterministic CUDA correctness fixture with explicit allocation, shape, iteration, and wall-time bounds.
3. Compute Sanitizer on the tiny fixture: `memcheck` first; then `initcheck`, `synccheck`, and `racecheck` when their corresponding memory/synchronization behavior is present.
4. Short attended microbenchmark with one GPU job, a timeout, telemetry sampling, and a cooldown.
5. Admission-only calculation for the exact model cell; do not load the model merely to discover infeasibility.
6. Short admitted 9B warmup/decode cell with quality and recovery checks.
7. Longer evidence run only after all earlier artifacts pass and the hardware supervisor is active.

- Scale allocations and shapes geometrically; never jump from a synthetic fixture directly to the maximum cap or model shape.
- Only one PrismInfer GPU test may run at a time. Do not combine a CPU stress run with a GPU stress run unless concurrency itself is the approved test and both are supervised.
- Treat illegal address/instruction, misalignment, device assertion, launch timeout/failure, GPU lost, or reset-required errors as context-fatal. Terminate the contained worker, preserve evidence, cool down, and require review before another hardware run.
- Never credit pagefile capacity as physical RAM. Pin only bounded transfer buffers; pinned host memory has a separate nonzero cap. Do not pin an entire model.
- Storage experiments use a bounded workspace scratch directory only. Do not write raw devices, alter the pagefile, or fill the OS volume; declare expected bytes and retain free-space headroom.

## PrismInfer Safe C++20 Profile

Use the C++ Core Guidelines as the general baseline. Apply the intent of NASA/JPL's Power of Ten to the small safety kernel: allocation accounting, admission, artifact parsers, process launch, hardware supervision, CUDA launch wrappers, and recovery. Do not claim literal Power-of-Ten compliance; its original rules target mission-critical C and some blanket bans conflict with C++ RAII, llama.cpp, CUDA, and the planned provider ABI.

- Prefer compile-time type/bounds checks. Anything not checkable at compile time must carry enough extent, ownership, lifetime, and provenance information for release-active runtime validation.
- Use RAII and move-only handles for CUDA/host/pinned allocations, streams, events, NVML sessions, Win32 handles, mapped files, threads, and Jobs. No raw owning pointers and no direct `new`, `delete`, `malloc`, or `free` outside an audited resource wrapper.
- Use `std::span` or an equivalent sized view. Pointer-plus-length C/CUDA/GGML/Win32 boundaries must document ownership, extent, alignment, mutability, address space, and lifetime. Localize and justify pointer arithmetic and casts.
- Central checked helpers are mandatory for every byte, token, dimension, stride, offset, alignment, grid, transfer, and allocation add/multiply/ceil-div/narrowing operation. Overflow or non-finite numeric input fails closed before allocation or launch.
- Initialize every object and buffer. Do not rely on indeterminate storage, implicit padding, or device memory contents.
- Do not use `goto`, `setjmp`, `longjmp`, or direct/indirect recursion in the runtime, parser, governor, telemetry, watchdog, process, or kernel safety paths. Bounded recursion in offline tooling requires explicit review and a depth limit.
- Every input-derived loop, retry, queue traversal, poll, and backoff requires a validated finite cap, deadline, or cancellation path. Tensor loops use validated admitted dimensions.
- After a plan is admitted, decode/dispatch/watchdog safety paths may use only predeclared arenas/workspaces; no hidden or unbounded allocation. Offline calibration may allocate through RAII under an explicit budget.
- Keep safety-critical functions focused; about 60 logical lines is a review/decomposition trigger, not a formatting quota. Declare data at the smallest scope and prefer immutable state.
- Check every significant return/status from CUDA, NVML, DXGI, Win32, allocation, filesystem, stream, thread, synchronization, and backend APIs. An intentionally ignored result must be explicit, locally justified, and safe.
- Immediately check `cudaGetLastError()` after a launch. At deterministic test/calibration/request boundaries, synchronize the relevant stream/event and check the asynchronous result.
- Use side-effect-free assertions for programmer invariants. Untrusted data, device state, memory budgets, and recoverable failures require release-active validation and typed error/status propagation; debug assertions are never the sole guard.
- Expected failures use typed results/status and important results should be `[[nodiscard]]`. Exceptions must not cross C ABI, CUDA callback, provider ABI, thread-entry, or process boundaries.
- Minimize the preprocessor. Platform/CUDA feature switches are allowed when centralized and every supported configuration is tested; macro-hidden ownership, pointer dereference, or control flow is prohibited.
- Function pointers are allowed only in a versioned, immutable, validated provider/dispatch table with explicit lifetime and fallback tests. They are not a general escape from analyzable control flow.
- Do not use `volatile` for synchronization. Use standard atomics, locks, immutable snapshots, or message passing with documented memory ordering.
- Safety suppressions must name the exact rule, scope, rationale, compensating test, owner/reviewer, and removal condition. Blanket project/file suppressions are prohibited.

## Native Process, Artifact, and Provider Boundary

- Do not invoke model runtimes through `std::system`, `system`, `popen`, `cmd /c`, PowerShell command strings, or shell-expanded model/user input.
- On Windows, use a fully qualified approved executable with `CreateProcessW`-class native argument handling, a controlled environment/current directory, restricted handle inheritance, and secure DLL search behavior.
- Create the worker suspended, assign it to a non-breakaway Job Object with kill-on-close and explicit memory/time/process limits, then resume. Account for the complete child tree and apply a wall-clock timeout.
- Treat GGUF, manifests, sidecars, JSONL, traces, caches, model paths, derived artifacts, and provider libraries as untrusted. Enforce maximum file/record/string/rank/dimension/tensor/nesting/decoded-allocation sizes before materialization.
- Use canonical trusted roots, an explicit reparse-point policy, opened-handle final-path/file-identity verification, and post-open hashing/size checks. Never validate a path and later reopen it by name for execution.
- Use opaque run IDs and trusted output roots. Write to a bounded temporary file, flush, and publish by same-volume replacement. Do not place prompt/model-provided text in a path.
- Pin and record source, executable, DLL, model, quant, tokenizer, prompt-fixture, and provider hashes. Do not load arbitrary plan-selected executables or DLLs.
- Do not retain model weights, prompts, activations, or sensitive paths in logs unless explicitly approved by the retention policy.

## Verification Gates by Change Risk

- **T0 documentation/tracker:** run changed-file Markdown/link/schema checks, Plan/Project synchronization when applicable, and concise whitespace validation. Independent review is required only when a governing safety or claim contract changes.
- **T1 ordinary CPU code:** run the smallest deterministic build/test slice at each checkpoint and the complete applicable CPU/hosted lane once at the packet head. Packet-level independent review may cover all unchanged T1 checkpoints.
- **T2 safety-critical code or governance:** admission/allocation arithmetic, untrusted parsers, native worker/supervisor, recovery, WDDM/DXGI evidence, workflows, provider boundaries, CUDA, and safety-contract changes require focused negative tests plus independent exact-head diff/security review before merge.
- **T3 hardware/model evidence:** use one exact reviewed commit, the applicable clearance, and a separate explicit user authorization describing commands, artifacts, limits, duration, watchdog/abort conditions, and evidence. A code or policy change after approval invalidates only the affected hardware receipt and requires fresh authorization.
- Risk tiers select the minimum sufficient gate; they never waive a stronger gate imposed by a changed subsystem, issue contract, clearance row, or reviewer finding.

- All first-party targets—not only `prisminfer_core`—must eventually share strict warnings. Changed first-party code must be warning-clean under MSVC `/W4 /permissive-` and the Clang/GCC equivalents; safety-critical promotion additionally requires warnings-as-errors and `/sdl` or an equivalent hardened lane.
- Run clang-tidy/MSVC analysis for changed safety code with relevant `clang-analyzer`, `bugprone`, `cert`, `cppcoreguidelines`, and `concurrency` checks. Suppressions follow the rule above.
- Add/maintain a non-CUDA ASan lane for parsers, ledgers, planners, process/artifact code, and bounded fuzz fixtures. Add UBSan on a supported Clang lane. Sanitizer binaries are test artifacts, not production deliverables.
- Every changed CUDA kernel requires a retained CPU or trusted-library reference and tests for rejected zero/empty shapes, size one, non-tile multiples, maximum admitted shape, alignment, aliasing, and bounds.
- Compute Sanitizer `memcheck` is mandatory before CUDA promotion. Use `racecheck` for shared-memory/asynchronous-copy changes, `initcheck` for initialization changes, and `synccheck` for synchronization changes. Instrumentation can trigger WDDM timeouts, so use only tiny bounded fixtures.
- Correctness, sanitizer, cap, quality, recovery, and cleanup gates precede profiling. Fast math, lower precision, reordered reductions, or approximation require separate numerical-quality evidence.
- No performance or memory claim survives a failed/absent safety lane, requested-versus-actual path acknowledgement, telemetry reconciliation, or cleanup artifact. Canonical commands are listed once under [Verification and Development Commands](#verification-and-development-commands).

## Hardware CI and Workflow Policy

- `.github/workflows/cmake-ci.yml` is hosted default CI and must not require CUDA. Hardware workflows require `self-hosted`, `Windows`, `NVIDIA`, and `CUDA` labels.
- Persistent self-hosted hardware runners must never use `pull_request`, `pull_request_target`, or fork-triggered execution. Hardware workflows are manual `workflow_dispatch` from an explicitly reviewed trusted commit. Prefer an ephemeral, non-admin runner with constrained network access and no unrelated credentials.
- Hardware jobs use read-only permissions, no repository/cloud secrets, `persist-credentials: false`, an exclusive GPU lock, a clean bounded workspace, and explicit job/test timeouts. Pin third-party Actions by full commit SHA before evidence can be promoted.
- Skipped, unavailable, or manually bypassed safety work cannot emit promotable evidence. A unit-test exception must classify the complete run as non-promotable rather than exit successfully as if validated.
- Do not expose a self-hosted runner to untrusted changes merely because the GitHub token is read-only; checked-out code has the host account's OS permissions.
- Run `actionlint` for workflow edits; `scripts\verify.ps1` does this when installed.

## Codex Skills and Plugin Routing

- Use `github:github` for issue/PR/Project #2 context, `github:gh-fix-ci` for failing Actions, `github:gh-address-comments` for review feedback, and `github:yeet` only when the user asks to publish changes.
- Use `codex-security:threat-model` when process/artifact/provider/runner trust boundaries change; use `codex-security:security-diff-scan` before merging such changes and a repository `security-scan` or `deep-security-scan` at a declared phase/security exit.
- Hugging Face skills are limited to pinned research/artifact/evaluation work; NVIDIA deployment skills do not replace local `nvidia-smi`, sanitizer, WDDM/DXGI, reference, or profiler evidence.
- Skills may automate checks but cannot weaken this file, `Plan.md`, V2 owner contracts, tests, schemas, or phase contracts. They never authorize installs, hardware-state changes, model downloads/Jobs/execution, publishing, or external uploads without the user's exact approval.

## Verification and Development Commands

Run only the lane authorized by the change and hardware ladder:

```powershell
# Default CPU verification
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1

# Optional CUDA probe. This remains distinct from the established #73 tiny
# synthetic-kernel correctness lane; both lanes remain opt-in and bounded.
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1 -WithCuda

# Non-mutating setup/cleanup helpers
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-setup.ps1 -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-setup.ps1 -WithCuda -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev-clean.ps1 -WhatIf
```

- Default Windows local builds are non-CUDA and use `scripts\verify.ps1`, whose `-BuildJobs 0` default estimates bounded parallelism from freshly sampled physical/commit payload, the T-101 development reserves, and a conservative 2 GiB/job envelope. This is a non-promotable preflight, not a process-tree cap or live watchdog. An explicit `-BuildJobs 1..8` is only an upper cap and cannot bypass the memory-derived bound; hosted CI may retain eight jobs only on its known runner class. On Linux/macOS use the README's serial CPU-only CMake/CTest path. Focused tests may use `ctest --test-dir build -C Debug -R "test_name" --output-on-failure`.
- CUDA kernels remain opt-in. PR #105 established the VS 2026 #73 synthetic fixture and its manual reviewed-main workflow, but it remains a serial, attended, non-promotable correctness/sanitizer lane only. Do not treat it as a model, performance, or deployment clearance.

## Code and Evidence Map

- `prisminfer_core` in `CMakeLists.txt` is the main library. Public headers live under `include/prisminfer/`; implementation is organized under `src/governor/`, `telemetry/`, `benchmark/`, `kv/`, `offload/`, `quality/`, and `kernels/`.
- Tools live under `tools/`; tests are deterministic CTest executables declared in `CMakeLists.txt`. Link shared code rather than duplicating it in tools.
- Validate probe JSONL with `prism-validate-lifecycle`; `scripts\verify.ps1` runs CPU smoke and forced-breach cases.
- Test breaches with simulation flags such as `--simulate-allocator-peak-bytes`, never excessive real allocation. Device-memory delta is corroboration only; certification requires reconciled allocator/process/backend/workspace evidence.
- Generated JSONL/manifests, build trees, and CTest logs are not source unless deliberately retained as fixtures.

## Project, Phase, and Claim Tracking

- Use repository issues/PRs and Project #2, `PrismInfer Roadmap`, as the current operational tracker. `Plan.md` is the program-control source established by #79 and Project #2 is its operational mirror. Tracker status never waives clearance.
- After completing a milestone, task, issue, or PR prep, update the related GitHub Project item status/fields before handoff; do not leave local-only progress as the only record.
- If no matching issue/project item exists for completed work, create or link one before marking roadmap progress. Keep PRs linked with `Closes #<issue>` or `Refs #<issue>` when an issue exists.
- Project automation may add new issues/PRs when `PROJECT_TOKEN` is configured, but agents should still verify the project item and set status/phase fields deliberately.
- Current packet, issue, and clearance status comes only from `Plan.md` and its Project #2 mirror. Before work, map the issue to the M0-M7 contract in [`docs/adaptive-runtime-v2/major-milestones.md`](docs/adaptive-runtime-v2/major-milestones.md); do not copy live status into this file. Profiler, custom-kernel, compression, routing, speculation, or KV evidence is required only for the corresponding Plan-admitted claim.
- Cross-cutting #109 defines workload-relative host admission and authoritative Windows system-commit telemetry for #82/#103/#84. Its development lane is always non-promotable, and it grants no CUDA, model, calibration, or C2 clearance.
- Keep the GPU hard-cap claim ceiling at 16 GiB (`17179869184` bytes) unless a task explicitly changes cap policy across configs, schemas, docs, and tests. Runtime admission is always lower when physical/budget/free-memory evidence minus reserve is lower.
- Source/tests describe current behavior when docs drift, but can never override `AGENTS.md` safety or an applicable clearance contract. Update conflicting docs within the scoped task.

## Safety References

- C++: [Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines), [NASA/JPL Power of Ten](https://spinroot.com/gerard/pdf/P10.pdf) as an adapted—not literal—profile, and [SEI CERT C++](https://wiki.sei.cmu.edu/confluence/display/cplusplus).
- CUDA/sanitizers: [CUDA Best Practices](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/index.html), [Compute Sanitizer](https://docs.nvidia.com/compute-sanitizer/ComputeSanitizer/index.html), [MSVC ASan](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-building), [Clang ASan](https://clang.llvm.org/docs/AddressSanitizer.html), and [UBSan](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html).
- Windows/CI: [TDR](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/timeout-detection-and-recovery), [DXGI budgets](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/ns-dxgi1_4-dxgi_query_video_memory_info), [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw), [Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects), and [GitHub Actions secure use](https://docs.github.com/en/actions/reference/security/secure-use).

## Worktree Hygiene

- `.local-archive/` is ignored, local-only, disposable, and non-canonical; durable historical material belongs under the tracked `docs/archive/` policy. `docs/archive/adaptive-runtime-v1-2026-07-11/` is immutable historical evidence and not active authority. Active documents and packet branches must use `docs/adaptive-runtime-v2/` and must not restore or link to the former V1 active directory.
- This repo often has local OpenCode/session files. Do not stage `.omo/`, `.clangd`, `compile_flags.txt`, build directories, probe artifacts, or generated manifests unless the user explicitly asks.
- Before any commit or PR prep, inspect `git status --short --untracked-files=all`, `git diff`, and recent log; stage only files for the scoped slice.
- Normal work is issue/PR based for `Gravelaw/prisminfer`; link PRs with `Closes #<issue>` or `Refs #<issue>` when an issue exists.
