# Phase 3 Implementation Plan: Transfer-Inclusive Offload Profitability

Phase 3 starts only after Phase 1 backend governance and Phase 2 KV accounting
exist. Its purpose is to decide when bounded GPU use, CPU staging, or NVMe
offload is actually worth using after transfer costs are included.

## Phase 3 Goal

Measure CPU-only, GPU-probed, CPU-offloaded, and NVMe-offloaded profiles using a
transfer-inclusive profitability rule.

## Non-Goals

- No isolated kernel-speed claim without end-to-end measurement.
- No offload success claim from launch success alone.
- No GPUDirect Storage path unless a later issue explicitly adds and validates
  it.
- No deployable 90B claim; Phase 4 owns 90B validation.
- No unified-memory oversubscription as certified hard-cap evidence.

## Architecture Direction

Phase 3 should add measurement and policy first, not an aggressive scheduler.
Profitability comparisons are valid only within the same validation cell.

Proposed abstractions:

```cpp
struct TransferSample {
  std::uint64_t h2d_bytes;
  std::uint64_t d2h_bytes;
  std::uint64_t nvme_read_bytes;
  std::uint64_t nvme_write_bytes;
  std::uint64_t pinned_host_peak_bytes;
  std::uint64_t staging_peak_bytes;
  double h2d_ms;
  double d2h_ms;
  double io_ms;
  double wait_ms;
};

struct ProfitabilityBaseline {
  double cpu_time_to_first_token_ms;
  double cpu_decode_tokens_per_second;
  std::uint64_t cpu_peak_bytes;
};

struct ProfitabilityDecision {
  bool accepted;
  std::string reason;
  double speedup_ratio;
  double required_speedup_ratio;
};
```

Initial policies:

- `cpu-only-baseline`
- `gpu-probed-baseline`
- `bounded-layer-offload`
- `host-kv-offload`
- `nvme-simulated`
- `nvme-experimental`

## Lifecycle v0.4 Target

Phase 3 adds transfer and profitability events:

```text
baseline_selected
transfer_plan
transfer_sample
offload_sample
profitability_result
```

Target sequence:

```text
run_start
config_validated
telemetry_probe
model_sidecar_validated
backend_selected
dependency_pins_resolved
cuda_context_probe, if GPU requested
cap_semantics_resolved
host_prepare
model_load_plan
backend_init
backend_warmup
kv_cache_profile
baseline_selected
transfer_plan
prefill_start
transfer_sample
prefill_end
decode_start
decode_token, repeated
transfer_sample, repeated as needed
decode_end
quality_gate_result
profitability_result
memory_sample
cap_certification_result
completed | failed_closed
run_end
```

## Config and CLI Additions

Add config fields:

```json
{
  "profitability_policy": "off|warn|fail-closed",
  "baseline_manifest": "",
  "min_speedup_ratio": 1.1,
  "offload_policy": "none|gpu|host-kv|nvme-simulated|nvme-experimental",
  "pinned_host_budget_bytes": 0,
  "staging_buffer_bytes": 0,
  "transfer_metrics": true,
  "cold_cache_run": false
}
```

Add CLI flags:

```text
--profitability-policy off|warn|fail-closed
--baseline-manifest PATH
--min-speedup-ratio R
--offload-policy none|gpu|host-kv|nvme-simulated|nvme-experimental
--pinned-host-budget-bytes N
--staging-buffer-bytes N
--transfer-metrics on|off
--cold-cache-run
```

## Manifest v0.4 Additions

Record:

- baseline manifest path/hash,
- validation cell id,
- CPU-only baseline metrics,
- offload policy,
- pinned host memory budget,
- staging buffer budget,
- H2D/D2H bytes,
- NVMe read/write bytes,
- copy wait time,
- prefill wall time,
- decode wall time,
- time-to-first-token,
- p50/p95 token latency,
- profitability decision,
- rejection reason.

## File and Module Candidates

New files:

```text
include/prisminfer/transfer_metrics.h
include/prisminfer/offload_planner.h
include/prisminfer/profitability_policy.h
include/prisminfer/cuda_device_profile.h
include/prisminfer/host_memory_tracker.h
src/benchmark/transfer_metrics.cpp
src/offload/offload_planner.cpp
src/governor/profitability_policy.cpp
tests/unit/test_transfer_metrics.cpp
tests/unit/test_offload_planner.cpp
tests/unit/test_profitability_policy.cpp
configs/benchmark-cpu-baseline.json
configs/benchmark-offload-profitability.json
docs/phase3-evidence.md
```

Likely modified files:

```text
CMakeLists.txt
include/prisminfer/config.h
include/prisminfer/runtime_state.h
include/prisminfer/benchmark.h
src/config.cpp
src/benchmark/manifest_writer.cpp
tools/prism-probe/main.cpp
tools/prism-validate-lifecycle/main.cpp
schemas/telemetry.schema.json
schemas/benchmark_manifest.schema.json
.github/workflows/cuda-probe-self-hosted.yml
docs/risk-register.md
```

## Detailed Implementation Slices

### Planner Agent Slice Map

| Slice | GitHub issue candidate | File/module candidates | Gate |
|---|---|---|---|
| P3-01 | Add transfer metrics model | `include/prisminfer/transfer_metrics.h`, `src/benchmark/transfer_metrics.cpp` | H2D, D2H, NVMe, wait, prefill, and decode timing fields serialize. |
| P3-02 | Add profitability policy | `include/prisminfer/profitability_policy.h`, `src/governor/profitability_policy.cpp` | Slower offload paths are rejected with explicit reason. |
| P3-03 | Add offload planner contract | `include/prisminfer/offload_planner.h`, `src/offload/offload_planner.cpp` | Plans declare placement, staging bytes, pinned host bytes, and bounded budgets. |
| P3-04 | Add CPU and GPU/offload benchmark configs | `configs/benchmark-cpu-baseline.json`, `configs/benchmark-offload-profitability.json` | Baseline and candidate use the same model, prompt, context, and output budget. |
| P3-05 | Extend telemetry/manifest to lifecycle v0.4 | `schemas/telemetry.schema.json`, `schemas/benchmark_manifest.schema.json`, `src/benchmark/manifest_writer.cpp`, `tools/prism-validate-lifecycle/main.cpp` | `baseline_selected`, `transfer_plan`, `transfer_sample`, and `profitability_result` validate in order. |
| P3-06 | Add self-hosted profitability workflow | `.github/workflows/offload-profitability-self-hosted.yml` | Artifacts include accepted and rejected runs. |
| P3-07 | Phase 3 evidence and audit | `docs/phase3-evidence.md`, `docs/risk-register.md` | Claim is allowed only when cap, quality, and profitability all pass. |

### P3-A: Comparable Benchmark Baselines

Purpose: make CPU-only, GPU-probed, and offload runs comparable by manifest
rather than by ad hoc logs.

Add:

```text
include/prisminfer/benchmark_comparator.h
src/benchmark/benchmark_comparator.cpp
tests/unit/test_benchmark_comparator.cpp
tools/prism-compare-benchmarks/main.cpp
docs/phase3-profitability-policy.md
```

Responsibilities:

- verify same model hash, prompt set, context length, output budget,
  quantization, validation cell, and quality gate before comparing runs,
- reject comparisons across incompatible manifests,
- compute time-to-first-token, decode throughput, p50/p95 token latency, and
  peak memory deltas,
- emit a machine-readable profitability verdict.

Acceptance:

- incompatible baselines fail closed with explicit reasons,
- slower GPU/offload path is rejected under `fail-closed`,
- warn mode records the rejection without promoting the run.

### P3-B: Transfer and Staging Accounting

Purpose: prevent offload claims that ignore PCIe, pinned host memory, staging
buffers, and NVMe traffic.

Add or extend:

```text
include/prisminfer/transfer_metrics.h
include/prisminfer/offload_planner.h
src/benchmark/transfer_metrics.cpp
src/offload/offload_planner.cpp
tests/unit/test_transfer_metrics.cpp
tests/unit/test_offload_planner.cpp
```

Responsibilities:

- record H2D/D2H bytes and wait time,
- record NVMe read/write bytes for NVMe policies,
- account for pinned host and staging-buffer budgets,
- record working set/RSS, private bytes, commit/pagefile pressure where
  available, and process IO bytes,
- keep cold-cache and warm-cache results separate.

Acceptance:

- NVMe policy without IO bytes is rejected,
- pinned-host budget breach fails closed,
- missing driver mode or TDR assumptions prevents long-running offload
  validation,
- cold-cache omission prevents deployable offload claims.

### P3-C: Self-Hosted Profitability Evidence

Purpose: produce CI-visible evidence on the Windows/NVIDIA runner without
making hosted CI depend on local hardware.

Add or extend:

```text
.github/workflows/cuda-probe-self-hosted.yml
.github/workflows/offload-profitability-self-hosted.yml
configs/benchmark-cpu-baseline.json
configs/benchmark-offload-profitability.json
docs/phase3-evidence.md
```

Responsibilities:

- run CPU baseline and GPU/offload smoke profiles on `self-hosted`, `Windows`,
  `X64`, `NVIDIA`, `CUDA`,
- upload manifests, telemetry, and comparator output,
- keep hosted CI focused on deterministic unit tests.

Acceptance:

- evidence includes at least one accepted or explicitly rejected profitability
  decision,
- artifact names contain run id and commit SHA,
- missing CUDA runner leaves hosted CI green but marks hardware evidence absent.

## Issue Slices

Create a Phase 3 milestone:

```text
Phase 3: Transfer-Inclusive Offload Profitability
```

Suggested issues:

1. Define transfer metrics and profitability data model.
2. Add transfer/profitability config and CLI contract.
3. Add transfer lifecycle and manifest v0.4 fields.
4. Add CPU-only baseline benchmark config.
5. Add GPU-probed baseline benchmark config.
6. Add bounded pinned-host staging budget tracking.
7. Add host KV offload experiment policy.
8. Add NVMe simulated policy with explicit non-deployable label.
9. Add profitability policy and rejection reasons.
10. Add self-hosted profitability evidence workflow.
11. Create Phase 3 evidence and exit audit.
12. Add Windows WDDM/TDR and host-pressure evidence gates.

## Test Gates

Hosted CI:

- Transfer metrics formatting/unit tests.
- Profitability decision tests.
- Config parser tests.
- Manifest writer tests.
- Lifecycle validator tests.

Self-hosted CUDA CI:

- CPU-only baseline artifact.
- GPU-probed baseline artifact.
- Transfer metrics smoke.
- Profitability policy smoke.
- Forced rejection when GPU/offload is slower than baseline.
- Rejection when manifests belong to different validation cells.

Manual/local evidence:

- Cold-cache and warm-cache comparison.
- Driver mode and WDDM/TDR assumptions recorded.
- Real model benchmark only with external pinned artifacts.

## Profitability Rules

An offload path is accepted only if:

- cap certification passes,
- quality gate passes,
- end-to-end time-to-first-token or decode throughput beats baseline by the
  configured threshold,
- transfer bytes and wait time are recorded,
- CPU-only baseline uses the same model, prompt, context, and output budget.
- CPU-only baseline and candidate run use the same validation cell.
- absolute usability thresholds pass: TTFT, sustained decode tokens/sec, p95
  token latency, and repeatability variance.

Reject if:

- only kernel timing improves,
- H2D/D2H transfer is missing,
- NVMe bytes are missing for NVMe policy,
- cold-cache behavior is not recorded for NVMe claims,
- warm-cache-only results are used as NVMe viability proof,
- host memory, pinned host memory, or staging pressure is unmeasured,
- driver mode/TDR evidence is missing for long-running Windows CUDA claims,
- cap telemetry is incomplete or contradictory.

## Implementation Order

1. Transfer metrics model and tests.
2. Profitability policy model and tests.
3. Config and CLI additions.
4. Manifest/lifecycle v0.4 additions.
5. CPU-only baseline config and evidence.
6. GPU-probed baseline config and evidence.
7. Pinned host staging budget tracking.
8. Host KV offload experiment policy.
9. NVMe simulated policy.
10. Self-hosted profitability workflow.
11. Phase 3 evidence document and exit audit.

## Exit Artifacts

Required artifacts before opening Phase 4:

- `docs/phase3-profitability-policy.md`
- `docs/phase3-evidence.md`
- CPU-only baseline manifest and telemetry,
- GPU-probed or offload manifest and telemetry,
- `prism-compare-benchmarks` output,
- self-hosted CUDA workflow artifact or documented hardware-evidence waiver.

## Exit Criteria

Phase 3 exits only when:

- CPU-only and GPU/offload profiles are comparable through manifests.
- Transfer metrics are included in the profitability decision.
- Comparisons are rejected when validation cells differ.
- Slower GPU/offload runs are rejected or clearly labeled.
- Pinned host and staging buffers are budgeted and reported.
- Cold-cache and warm-cache distinctions are explicit for NVMe paths.
- Self-hosted CUDA evidence includes accepted and rejected profitability cases.
