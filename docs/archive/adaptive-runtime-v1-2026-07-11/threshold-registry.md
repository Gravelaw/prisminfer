# Threshold and Sampling Registry

Registry version: `adaptive-thresholds-v0.3-provisional`
Status: proposed policy values; not distribution-free guarantees

This registry centralizes thresholds that were previously scattered across
phase and council documents. A threshold can promote a claim only when its
metric, experimental unit, population, sample plan, confidence method, and stop
rule are frozen before confirmatory measurement.

## Statistical Rules

- Distinguish kernel trial, request, prompt, decode sequence, token within a
  sequence, session, cold start, and machine restart.
- Tokens inside one sequence are correlated and do not count as independent
  request samples for TTFT or request-tail inference.
- Three end-to-end runs support a coarse repeatability/CV check; they do not
  estimate p95/p99 request tails reliably.
- Use nested partitions: calibration/search, model selection/pruning,
  confirmatory replay, and sealed promotion.
- A candidate modified after sealed promotion consumes a new sealed set.
- Adaptive candidate search requires multiple-comparison control or a fresh
  confirmatory replay of finalists.
- Report confidence intervals for differences and tail quantiles, not only
  pass/fail.
- Report task strata and worst-stratum behavior.
- Thresholds may change only through a versioned decision before rerun.

## Registry Fields

```text
threshold_id and version
metric definition and unit
population/validation cell
experimental unit
direction and practical rationale
provisional threshold
minimum detectable effect or precision target
confidence level and interval/test method
multiplicity policy
minimum sample-size or frozen sequential stop rule
calibration/confirmation/promotion split
owner and approving gate
status = provisional | validated-policy | retired
```

## Runtime Safety and Admission Thresholds

These are release-active provisional stop thresholds for P6-04A (GitHub issue
#103), not performance targets. They must be implemented in the trusted outer
supervisor and validated with deterministic fault injection before clearance C2.
Until then, only C0 work and conditionally permitted C1 tiny attended CUDA
fixtures are allowed. A threshold breach rejects or terminates the run; an
optimizer cannot trade it against throughput or quality.

| ID | Provisional threshold | Enforcement | Provisional owner and approval gate |
|---|---|---|---|
| T-100 | `policy_ceiling=16 GiB`; pre-context live capacity uses physical/reportable local bytes and DXGI budget; post-context also includes `owned+cuda_free`; `gpu_reserve=max(1 GiB, ceil(10% * pre_live), ceil(10% * post_live))`; the post-context effective cap is the minimum of the requested tier, pre-context admitted cap, and post-context reserve-adjusted live cap. | Every stage requires `peak <= effective_cap`; reserve is never payload. The admitted run cap can shrink under the watchdog but cannot grow during a run. | P6-04A/#103 runtime-safety owner; GPU architect plus safety reviewer approve any validated replacement. |
| T-101 | No fixed minimum available RAM is permitted. `development_nonpromotable` retains physical `max(2 GiB, ceil(8% * physical_total))` and commit `max(2 GiB, ceil(5% * commit_limit))`. `evidence_promotable` retains physical `max(4 GiB, ceil(15% * physical_total))` and commit `max(4 GiB, ceil(10% * commit_limit), ceil(15% * physical_total))`. Explicit reserves may raise but never lower the lane floor. Total PrismInfer pinned-host bytes are capped at `min(512 MiB, floor(2% * physical_total))` and count in both planned peaks. | Admit only the exact planned incremental resident and commit peaks plus uncertainty against their separate live payloads. Development results are always non-promotable. Reject before allocation and cancel on hard-reserve breach; pagefile/commit never increases physical payload. | #109 policy/primitive; P6-04A/#103 watchdog/token owner; CPU architect plus safety reviewer approve. |
| T-102 | GPU warning at `min(78 C, reported_target - 8 C)` and stop at `min(82 C, reported_target - 5 C, reported_slowdown - 5 C)` using every available bound. For a materially participating CPU with an approved package sensor, warn at `min(85 C, TjMax - 15 C)` and stop at `min(90 C, TjMax - 10 C)`. Restart only after every participating device is `<=70 C` continuously for 60 s and a fresh preflight. | Warning forbids scale-up. Stop immediately blocks new submissions and enters cancellation. Missing target/slowdown may use another GPU bound; a missing current sensor blocks C2+ for a materially participating device, except a separately approved short attended cell that proves the device is not a material load source. | P6-04A/#103 GPU/CPU owners; device-specific evidence and safety reviewer required before change. |
| T-103 | Supervisor sample period `<=100 ms`; worker heartbeat period `<=250 ms`; required guard sample or heartbeat age `<=500 ms`. | Stale/missing required evidence enters cancellation. Sampling must be outside the worker and record monotonic timestamps/drops. | P6-04A/#103 telemetry owner; runtime and GPU reviewers approve. |
| T-104 | New GPU work must show measured single-dispatch `p99 <250 ms` on the exact bounded fixture before scale-up; no individual admitted dispatch may use a declared bound above 500 ms. | Split/tile work that misses the gate. A timeout/device-lost result is context-fatal and cannot be retried automatically. This is a PrismInfer engineering margin, not a Windows TDR guarantee. | CUDA owner for measurement; P6-04A/#103 safety owner grants the exact-fixture gate. |
| T-105 | On breach, block submissions immediately; cooperative cancellation acknowledgement within 500 ms; worker exit within 2 s of request; otherwise terminate the Job; cleanup/lease reconciliation within 5 s; automatic same-context retry count `0`. | Supervisor timestamps every transition. Missed deadlines force abort/quarantine and review before another hardware run. | P6-04A/#103 process-safety owner; Windows/runtime safety reviewer approves. |

Definitions used by T-100 and T-101:

```text
pre_context_live_capacity_bytes = min(
  physical_or_reportable_local_bytes,
  dxgi_local_budget_bytes)

pre_context_gpu_reserve_bytes = max(
  1 GiB, ceil(0.10 * pre_context_live_capacity_bytes))

pre_context_effective_cap_bytes = min(
  requested_tier_bytes,
  max(0, min(policy_ceiling_bytes, pre_context_live_capacity_bytes)
    - pre_context_gpu_reserve_bytes))

post_context_live_capacity_bytes = min(
  physical_or_reportable_local_bytes,
  fresh_dxgi_local_budget_bytes,
  reconciled_owned_gpu_bytes + cuda_free_bytes)

gpu_reserve_bytes = max(
  pre_context_gpu_reserve_bytes,
  1 GiB,
  ceil(0.10 * post_context_live_capacity_bytes))

post_context_effective_cap_bytes = min(
  pre_context_effective_cap_bytes,
  requested_tier_bytes,
  max(0, min(policy_ceiling_bytes, post_context_live_capacity_bytes)
    - gpu_reserve_bytes))

effective_live_cap_bytes = post_context_effective_cap_bytes

current_live_capacity_bytes = min(
  physical_or_reportable_local_bytes,
  current_dxgi_local_budget_bytes,
  reconciled_owned_gpu_bytes + current_cuda_free_bytes)

current_reserve_adjusted_live_cap_bytes = max(
  0,
  min(policy_ceiling_bytes, current_live_capacity_bytes) - gpu_reserve_bytes)

watchdog_effective_cap_bytes = min(
  post_context_effective_cap_bytes,
  current_reserve_adjusted_live_cap_bytes)
```

Host admission uses system-wide `CommitTotal` and `CommitLimit` from Windows
`GetPerformanceInfo`, not the process-bounded `MEMORYSTATUSEX` pagefile fields:

```text
host_lane = development_nonpromotable | evidence_promotable

development_physical_reserve_bytes = max(
  2 GiB, ceil(0.08 * physical_total_bytes))
development_commit_reserve_bytes = max(
  2 GiB, ceil(0.05 * commit_limit_bytes))

evidence_physical_reserve_bytes = max(
  4 GiB, ceil(0.15 * physical_total_bytes))
evidence_commit_reserve_bytes = max(
  4 GiB,
  ceil(0.10 * commit_limit_bytes),
  ceil(0.15 * physical_total_bytes))

physical_reserve_bytes = max(
  lane_physical_reserve_bytes,
  explicit_physical_reserve_bytes)
commit_reserve_bytes = max(
  lane_commit_reserve_bytes,
  explicit_commit_reserve_bytes)

physical_payload_bytes = max(
  0, physical_available_bytes - physical_reserve_bytes)
commit_headroom_bytes = max(
  0, commit_limit_bytes - commit_total_bytes)
commit_payload_bytes = max(
  0, commit_headroom_bytes - commit_reserve_bytes)

required_resident_bytes =
  planned_incremental_resident_peak_bytes
  + resident_uncertainty_bytes
  + pinned_host_bytes
required_commit_bytes =
  planned_incremental_commit_peak_bytes
  + commit_uncertainty_bytes
  + pinned_host_bytes

host_admission = pass only when
  required_resident_bytes <= physical_payload_bytes
  and required_commit_bytes <= commit_payload_bytes
  and pinned_host_bytes <= pinned_host_cap_bytes
```

All arithmetic is checked and saturating subtraction is used only to produce a
zero payload before rejection. Missing/stale telemetry, a non-authoritative
commit source, overflow, contradictory totals, reserve breach, or a request
above either payload fails closed. A development receipt cannot be relabeled:
promotion requires a fresh `evidence_promotable` admission and token. Ordinary
CPU development may continue when its exact incremental request fits the
development lane; a plan that needs 24 GiB is rejected or downscaled without
disabling smaller plans merely because less than 24 GiB is currently free.

The three cap meanings are distinct: `policy_ceiling_bytes` defines product
scope, `requested_tier_bytes` defines the validation cell, and
`effective_live_cap_bytes` is the only executable peak bound. The nominal 16
GiB ceiling is never an allocation target. If a required live source cannot be
observed or reconciled, C2+ fails closed rather than dropping that term.
Pre-context does not fabricate a CUDA-free observation; that source becomes
mandatory only after the worker creates the minimal context.

## Foundation and 9B Thresholds

| ID | Metric and provisional policy | Independent unit / sample plan | Interpretation |
|---|---|---|---|
| T-001 | GPU peak <= T-100 effective live cap; requested tier <=16 GiB; unknown promoted GPU bytes = 0 | Every lifecycle sample plus allocator/backend peak evidence for each run | Hard cap, not a statistical trade-off; a requested 16 GiB tier is not permission to allocate 16 GiB. |
| T-002 | Fixture pass rate >=95% | Paired prompts stratified by task; sample size chosen for desired binomial interval before promotion | Project policy floor, not proof over all prompts. |
| T-003 | Task-quality regression <=5% | Paired prompt/task scores with per-stratum interval and worst-stratum report | Project policy floor. |
| T-004 | Warm decode p50 >=3 tokens/s | Independent decode sequences across sessions; report paired interval | Exact 9B cell usability floor. |
| T-005 | Request ITL p95 <=750 ms | Enough independent requests/sequences to achieve frozen quantile precision; multiple sessions | Do not infer from three sequences alone. |
| T-006 | TTFT p95 <=30 s | Independent cold/warm requests according to cell; enough cold starts for frozen quantile precision | Cold and warm cells separate. |
| T-007 | Sustained decode CV <=10% | At least three sessions/runs for diagnostic; more if used for inference | Repeatability diagnostic only. |
| T-008 | Orchestration overhead <=2% | Paired same-plan requests, randomized order, session blocks | Provisional practical budget. |

## Calibration and Selection Thresholds

| ID | Metric and provisional policy | Unit / plan | Interpretation |
|---|---|---|---|
| T-020 | Median held-out stage prediction error <=10% | Complete held-out actuator/shape observations across session blocks | Predictive gate, not causal attribution. |
| T-021 | p95 stage prediction error <=20% | Held-out combinations with quantile precision plan | Provisional abstention/accuracy gate. |
| T-022 | End-to-end p95 prediction error <=10% | Independent held-out requests across context/task strata | Required before automatic selection. |
| T-023 | Throughput regret <=5% median versus measured feasible oracle | Held-out request/cell, finalists confirmed fresh | Tests selector, not global optimality. |
| T-024 | Tail regret <=10% versus lowest-p95 feasible candidate | Independent requests with tail interval | Prevents average-throughput choice from harming tail excessively. |
| T-025 | Memory upper bound never underpredicts observed promoted peak | Every promoted run/cell | One-sided safety requirement. |

## Optional Mechanism Thresholds

All are provisional continuation policies. They are not Phase 7 dependencies.

| ID | Mechanism and threshold | Unit / plan | Rationale/status |
|---|---|---|---|
| T-040 | Kernel/adaptive speed claim >=1.10x end-to-end | Paired exact-cell requests/sessions; confidence interval excludes no improvement | Existing project continuation rule. |
| T-041 | Transfer overlap >=5% end-to-end phase benefit | Paired serialized/overlapped schedule plus timeline | Avoid complexity for invisible gain. |
| T-042 | mmap prefetch >=10% cold TTFT benefit | Independent cold starts, file/cache state verified | Provisional continuation. |
| T-043 | Lossless tile: >=15% effective reduction and >=10% exposed fetch+decode benefit | Independent randomized tiles plus full-run confirmation | Diagnostic stop gate. |
| T-044 | Static progressive weights: >=15% effective bytes saved and >=10% end-to-end benefit | Separate format/kernel experiment and quality set | Optional research, not phase exit. |
| T-045 | KV quantization: >=40% net bytes saved; codec cost <=80% of measured time saving | Independent sequences/context strata | Ensures positive margin; quality gates also required. |
| T-046 | Activation compression: timing inequality passes with >=20% p95 margin | Real boundary shapes/transfers plus end-to-end confirmation | Provisional research gate. |
| T-047 | Structured oracle removes >=30% aligned work at quality limit | Full-continuation counterfactual prompts across cells | Stop before router training if it fails. |
| T-048 | Router realizes >=95% of oracle savings and >=10% end-to-end gain | Nested training/validation/promotion plus OOD/audit | Approximate research; audit risk bound required. |
| T-049 | Joint plan >=10% over best static same-cell baseline | Only independently passing mechanisms; fresh confirmation | Joint speedup claim, not mandatory phase outcome. |

## Large-Model Viability Thresholds

The first step is an artifact-specific lower bound. Execution is admitted only
when its optimistic confidence/bound meets the frozen target.

Initial discovery policy:

| ID | Metric | Provisional target | Unit/plan |
|---|---|---:|---|
| T-060 | Committed output throughput | >=1.0 token/s | Independent target-distributed output sequences; speculation counts committed output, not accepted drafts alone. |
| T-061 | TTFT p95 | <=120 s | Independent requests/cold state as declared; quantile precision plan. |
| T-062 | ITL p95 | <=2 s | Independent sequences/requests, not tokens treated as iid. |
| T-063 | Throughput CV | <=10% | Multiple sessions; diagnostic plus interval. |
| T-064 | Quality regression | <=5% | Paired task strata and worst-stratum report. |
| T-065 | Memory/host/storage evidence | complete for claim class | Hard evidence gate. |

These are research viability targets, not deployment SLAs.

## Sample-Plan Template

Before a confirmatory run, record:

```text
metric and threshold IDs
independent experimental unit
expected baseline and variance/tail estimate from pilot data
minimum detectable practical effect or desired interval width
confidence level
power or precision calculation
number of sessions, requests, prompts, cold starts, and machine restarts
randomization/blocking strategy
multiple-comparison correction or finalist replay rule
missing/failure handling
sequential stop rule if used
sealed dataset/run identifiers
```

## Initial Implementation Gate

P6-04A/#103 must implement and fault-test T-100 through T-105 before C2 or any
model-backed Phase 6 execution. P7-00 may publish the remaining registry as
provisional. P7-07 replaces generic
"three-run p95" language with metric-specific sample plans derived from pilot
data. P7-10 cannot promote the optimizer until the relevant entries have an
approved frozen sample plan and confirmatory replay.
