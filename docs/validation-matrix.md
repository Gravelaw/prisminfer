# Validation Matrix

This document is the canonical validation envelope for PrismInfer model-size and
VRAM-tier claims.

## Current Envelope

Current GPU policy ceiling:

```text
16 GiB = 17179869184 bytes
```

No roadmap item, config, benchmark claim, or GitHub issue should request or
validate a GPU tier above 16 GiB until the cap policy is explicitly changed.
This is a product/claim ceiling, not an allocation target. The requested tier
and effective live cap are separate values and may be lower.

VRAM tiers:

```text
1 GiB, 2 GiB, 4 GiB, 6 GiB, 8 GiB, 12 GiB, 16 GiB
```

Model parameter buckets:

```text
<=2B
>2B-5B
>5B-10B
>10B-15B
>15B-20B
...
>85B-90B
```

## Cap Admission Equation

Every GPU cell records three non-interchangeable cap meanings:

```text
policy_ceiling_bytes = 17179869184
requested_tier_bytes = selected validation tier

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

effective_live_cap_bytes = min(
  pre_context_effective_cap_bytes,
  requested_tier_bytes,
  max(0, min(policy_ceiling_bytes, post_context_live_capacity_bytes)
    - gpu_reserve_bytes))
```

The reserve is provisional T-100 from the
[threshold registry](adaptive-runtime/threshold-registry.md). A cell is
executable only when its peak fits `effective_live_cap_bytes`. A configuration
that requests 16 GiB does not prove that 16 GiB exists physically or is
available under WDDM.

Admission is two-stage. The supervisor performs conservative admission before
worker CUDA-context initialization. After minimal context creation it
reconciles actual context/runtime bytes plus CUDA/DXGI observations and repeats
admission before model load. Model/backend allocation is reconciled before
warmup and watched throughout the run. Pre-context reject means no CUDA
context; post-context reject means no model load.
The watchdog may reduce the admitted effective cap as live conditions worsen;
it cannot grow the cap during a run.

## Host Admission Envelope

Host admission is workload-relative and independent from the requested GPU
tier. The canonical T-101 formulas are maintained in the
[threshold registry](adaptive-runtime/threshold-registry.md); this matrix never
uses a fixed free-RAM prerequisite such as 24 GiB.

For each decision, calculate separate live payloads:

```text
effective_physical_reserve_bytes = max(
  lane_physical_reserve_bytes, explicit_physical_reserve_bytes)

effective_commit_reserve_bytes = max(
  lane_commit_reserve_bytes, explicit_commit_reserve_bytes)

physical_payload_bytes = checked_subtract(
  physical_available_bytes, effective_physical_reserve_bytes)

commit_headroom_bytes = checked_subtract(
  system_commit_limit_bytes, system_commit_total_bytes)

commit_payload_bytes = checked_subtract(
  commit_headroom_bytes, effective_commit_reserve_bytes)

required_resident_bytes =
  planned_incremental_resident_peak_bytes
  + resident_uncertainty_bytes
  + pinned_host_bytes

required_commit_bytes =
  planned_incremental_commit_peak_bytes
  + commit_uncertainty_bytes
  + pinned_host_bytes
```

Admit only when each requirement fits its corresponding payload. Pagefile or
commit capacity cannot be credited as physical payload. On Windows, system
commit counters must come from `GetPerformanceInfo`; process-bounded
`MEMORYSTATUSEX` pagefile fields are not authoritative for this decision.

Required deterministic host cells are:

| Cell | Expected result |
|---|---|
| 32 GiB physical total, 64 GiB commit limit/16 GiB charge, development lane with 8 GiB physical available, and a bounded plan inside its payload | Admit as non-promotable. |
| Same pinned totals/charge, evidence lane with 8 GiB physical available, and a bounded plan inside its stricter payload | Admit as promotion-eligible only when promotion is requested; #103 and every other gate still control actual evidence promotion. |
| Same pinned totals/charge, evidence lane with 12 GiB and 15 GiB physical available | Admit or reject solely from the exact planned peaks and live payload, never the absence of 24 GiB free. |
| Adequate physical availability with insufficient system commit headroom | Reject on the commit reserve or planned commit peak. |
| Missing, stale, process-bounded, contradictory, mismatched, or overflowing host telemetry | Fail closed before allocation. |
| Development receipt presented for promotion | Reject; require fresh evidence-lane admission. |
| Pinned-host request above the T-101 cap | Reject before pinning. |
| Explicit per-run physical or commit reserve exceeds its lane floor | Use the higher effective reserve; reject if it consumes the reserve or planned payload. |

Each validation cell must account for the resident GPU memory envelope:

```text
peak_vram =
  resident_weight_bytes
+ resident_kv_bytes
+ weight_metadata_bytes
+ kv_metadata_bytes
+ kv_residual_or_sketch_bytes
+ dequant_workspace_peak_bytes
+ activation_workspace_bytes
+ cuda_context_runtime_bytes
+ kernel_workspace_peak_bytes
+ retained_pool_bytes
+ allocator_fragmentation_bytes
+ unknown_gpu_bytes
+ telemetry_safety_margin_bytes

certify only if peak_vram <= effective_live_cap_bytes
```

Device-level memory delta is corroborating evidence only. Certification requires
allocator/process/backend evidence appropriate to the claim label.

Compression-enabled cells must expand the equation instead of replacing it with
nominal bit-width claims:

```text
compression_peak_vram =
  cuda_context_runtime_bytes
+ resident_quant_weight_bytes
+ weight_metadata_bytes
+ weight_dequant_workspace_peak_bytes
+ activation_workspace_peak_bytes
+ kv_payload_bytes
+ kv_metadata_bytes
+ kv_residual_window_bytes
+ kv_rotation_or_projection_metadata_bytes
+ kv_reconstruction_workspace_peak_bytes
+ kernel_workspace_peak_bytes
+ allocator_fragmentation_bytes
+ retained_pool_bytes
+ unknown_device_bytes
+ telemetry_safety_margin_bytes
```

A compression profile cannot certify a constrained-VRAM claim while
`unknown_device_bytes`, unreconciled backend bytes, or hidden retained-pool bytes
remain unexplained.

## Cell Identity

Every benchmark or claim should identify a validation cell:

```text
validation_cell_id =
  model_parameter_bucket
  + model_hash
  + quant_artifact_sha256
  + tensor_quantization_inventory_sha256
  + context_tokens
  + vram_tier_gib
  + backend
  + os
  + hardware_class
```

Required manifest/config fields:

```text
model_parameter_bucket
parameter_count
model_hash
quantization_format
quant_artifact_sha256
quantization_recipe_id
mixed_quantization
tensor_quantization_inventory_sha256
context_tokens
vram_tier_gib
policy_ceiling_bytes
requested_tier_bytes
pre_context_live_capacity_bytes
pre_context_effective_cap_bytes
post_context_live_capacity_bytes
gpu_reserve_bytes
effective_live_cap_bytes
pre_context_admission_status
post_context_admission_status
clearance_stage
supervisor_build_sha256
worker_build_sha256
validation_cell_id
validation_cell_status
```

`vram_tier_gib` is a legacy/display alias for `requested_tier_bytes`; it is not
the executable cap. New manifests use the byte-valued fields as authoritative.
Promoted GPU/model evidence requires P6-04A/#103 supervisor, Job, lease,
watchdog, cancellation, and cleanup evidence for the declared clearance stage.

### Mixed-quant GGUF semantics

Labels such as `Q4_K_M` commonly identify an artifact recipe whose tensors may
use multiple GGML quantization types. Therefore `quantization_format=q4` or a
nominal average bit width is insufficient identity and insufficient capacity
evidence. Every GGUF model cell retains:

```text
gguf_artifact_sha256
quantization_label
quantization_recipe_id and tool/source revision
mixed_quantization = true | false
tensor_quantization_inventory = sorted tensor name, type, shape, offset, bytes
tensor_quantization_inventory_sha256
tensor_type_count_and_byte_histogram
weight_payload_bytes and weight_metadata_bytes from the exact artifact
```

Capacity uses exact tensor/artifact bytes, not parameter count multiplied by a
labelled bit width. A same-cell kernel comparison keeps the model and quant
artifact hashes plus inventory hash identical. An offline progressive or
alternative representation is a derived-artifact variant with parent hash,
recipe, effective bytes, reconstruction workspace, and quality evidence; it is
not silently the same quant cell. A static mixed-quant artifact identity does
not authorize runtime precision switching.

Compression-enabled cells add:

```text
compression_scope
compression_algorithm_family
payload_bits_per_value
effective_bits_per_value
metadata_bits_per_value
residual_window_tokens
rotation_or_projection_policy
reconstruction_workspace_peak_bytes
attention_logit_error_p95
attention_logit_error_p99
attention_topk_overlap
quality_gate_id
compression_artifact_sha256
```

Kernel benchmark cells add these fields when a claim involves optimized compute:

```text
op_type
sequence_phase
batch_size
matrix_m
matrix_n
matrix_k
kernel_backend
kernel_name
kernel_version
baseline_backend
dequant_fused
full_dequant_materialized
workspace_peak_bytes
driver_mode
tdr_status
application_control_status
correctness_fixture_hash
```

Kernel fields are part of cell identity for speed or efficiency claims. A
batch-1 decode GEMV result cannot promote a prefill GEMM result, a different
quantization format, a different GPU architecture, or a different driver mode.

Compression benchmark cells add these fields when a claim involves KV, vector,
or alternative numeric representation work:

```text
compression_policy
compression_algorithm_family
kv_payload_bytes_uncompressed
kv_payload_bytes_compressed
kv_metadata_bytes
kv_residual_or_sketch_bytes
effective_bits_per_value
compression_encode_time_ms
compression_decode_time_ms
attention_logit_error_p95
attention_logit_error_p99
attention_topk_overlap
quality_gate_id
compression_artifact_sha256
```

Compression implementation details are variant fields for same-cell comparison,
but model hash, quant artifact, context, batch, prompt fixture, OS, GPU, driver,
CUDA version, and cap tier must still match. A compression result with missing
quality evidence is rejected even when it saves memory.

For baseline-versus-candidate comparisons inside the same validation cell,
implementation fields such as `kernel_backend`, `kernel_name`,
`kernel_version`, and `compression_algorithm_family` are variant fields, not
model-cell identity fields. The comparator must still reject mismatches in
model hash, quant artifact, prompt fixture, context, batch, OS, GPU, driver,
CUDA version, backend class, and cap tier.

## Hardware Execution Clearance

Clearance controls which evidence a cell may collect; it is not a model result
status. The normative runtime transitions are in the
[runtime state machine](runtime-state-machine.md), and the entry evidence is in
the [execution plan](adaptive-runtime/execution-and-testing-plan.md).

| Clearance | Maximum permitted evidence |
|---|---|
| C0 | CPU/reference, simulation, schemas, and fault injection without GPU work. |
| C1 | Tiny attended deterministic CUDA correctness/Compute Sanitizer fixture; no model, calibration, sustained benchmark, or unattended execution. |
| C2 | Short supervised synthetic CUDA benchmark/calibration only after P6-04A/#103 and T-100 through T-105 pass. |
| C3 | Short exact-artifact model-backed Phase 6 evidence after C2 plus model/quant/quality prerequisites. |
| C4 | Sustained conventional 9B calibration/replay after the Phase 6 audit and Phase 7 entry gates; Ornith remains a separate admitted stress cell. |
| C5 | Longer evidence and separately admitted 30B/70B/90B work. |

An issue closed without retained exit evidence does not grant clearance. A
guard breach, context-fatal CUDA error, missed cancellation deadline, or
unreconciled cleanup makes the run `rejected` or quarantined and requires review
plus fresh preflight before another hardware attempt.

## Cell Status

Allowed statuses:

| Status | Meaning |
|---|---|
| `not-started` | No retained evidence. |
| `metadata-only` | Model/config/sidecar checks only. |
| `warmup` | Backend warmup evidence exists; no decode claim. |
| `decode-smoke` | At least one bounded decode run exists. |
| `quality-gated` | Quality gates pass for this exact cell. |
| `profitable` | Transfer-inclusive comparison beats baseline for this cell. |
| `validated` | Cap, quality, performance, repeatability, and manifest gates pass. |
| `rejected` | A declared gate failed. |

## Advancement Rule

Do not promote a larger model bucket or more aggressive claim by analogy. A pass
in one cell does not imply a pass in another cell.

Examples:

- A pass in `>2B-5B @ 4 GiB` does not imply `>5B-10B @ 4 GiB`.
- A 9B pass does not imply a pass for the whole `>5B-10B` bucket.
- A pass on one model family does not imply bucket-level generality.
- A 90B simulated plan does not imply measured or deployable inference.

Phase 1 should prove representative `<=2B` and `>2B-5B` backend cells before
larger buckets are claimed. Later phases expand the matrix through KV quality,
offload profitability, and large-model claim validation.

## Required Evidence Per Promoted Cell

Promoted cells require retained artifacts:

- telemetry JSONL,
- benchmark manifest,
- lifecycle validator result,
- clearance record and P6-04A/#103 exit evidence where C2+ applies,
- supervisor/worker hashes, exclusive lease, Job limits/exit, watchdog,
  cancellation/abort, cleanup, and cooldown evidence,
- dependency pin record,
- model hash, quantization artifact hash, recipe, and tensor inventory hash,
- backend and OS/hardware profile,
- policy ceiling, requested tier, live observations, reserve, effective cap,
  both admission decisions, and cap certification or precise fail-closed reason,
- host admission lane, authoritative counter source, physical/commit reserves,
  exact planned peaks, uncertainty, pinned bytes, payloads, and typed decision,
- quality gate result when the claim mentions useful output,
- host memory and IO telemetry when CPU/offload participates,
- transfer metrics when GPU/offload profitability is claimed,
- repeatability evidence for validated or deployable labels.

Kernel claims additionally require:

- CPU reference correctness result,
- same-cell llama.cpp/GGML CUDA/MMQ baseline where CUDA is involved,
- cuBLASLt or CUTLASS baseline when GEMM or Tensor Core claims are made,
- launch, sync, transfer, dequantization, and workspace measurements,
- profiler artifact path and hash when Tensor Core or occupancy claims are
  made,
- explicit fallback reason when a vendor or Tensor Core path is unavailable.

## Initial Representative Ladder

| Bucket | Candidate status target | Notes |
|---|---|---|
| `<=2B` | `warmup`, then `decode-smoke` | First real backend and schema migration target. |
| `>2B-5B` | `warmup`, then `decode-smoke` | Required before larger claims. |
| `>5B-10B` | `quality-gated` | First practical constrained-inference quality lane. Phase 5 must include a 9B-class constrained-VRAM kernel cell; exact-model only unless multiple representatives pass. |
| `>10B-15B` | `quality-gated` or `rejected` | 12B/13B class, likely CPU-heavy under low caps. |
| `>20B-30B` | `measured-offload` or `rejected` | 26B class; offload and host pressure become central. |
| `>65B-70B` | `simulated` or `measured-offload` | Never promote without full evidence. |
| `>85B-90B` | `simulated`, `measured-offload`, or `rejected` | Terminal scale bucket under the current <=16 GiB envelope. |

## Representative 9B Gate

The first concrete `>5B-10B` gate is a pinned 9B-class dense GGUF model.

This gate does not create a new model bucket. It is a representative cell inside
`>5B-10B`, and a pass does not generalize to every 9B model family,
quantization format, context length, GPU, driver mode, or VRAM tier.

Required fixed cell assumptions:

- `model_parameter_bucket`: `>5B-10B`
- `parameter_count`: exact model metadata value, approximately 9B
- `context_tokens`: `2048` for the first gate
- `batch_size`: `1`
- decode sample: at least `128` generated tokens per retained run
- quantization: pinned GGUF q4-family artifact, preferably `Q4_K_M` or the exact
  selected recipe, with the complete per-tensor type/shape/byte inventory;
  `Q4_K_M` is treated as potentially mixed-quant, not uniformly four bits
- no unproven runtime compression, low-rank reconstruction, sparsity, MoE
  pruning, or full FP16 materialization may be counted as part of this gate
- `model_hash` and `quant_artifact_sha256` are mandatory

The first 9B compression gate is separate from the first 9B q4 residency gate.
Q4 weight residency proves that weights can stay compressed. KV compression
then tests whether context-growth memory can be reduced without quality loss.
TurboQuant, PolarQuant, QJL, KIVI, KVQuant, and QServe-style policies must
start as exact-cell `reference` or `experimental` evidence until retained
quality, overhead, and cap manifests exist.

Runtime compression can be tested only as a separate exact-cell candidate. The
candidate must retain the uncompressed/q4 same-cell baseline and add
compression-specific evidence for effective bits, metadata overhead,
reconstruction workspace, attention error, top-k overlap, quality deltas, and
end-to-end performance.

9B VRAM-tier targets:

| Tier | Gate role | Allowed outcome |
|---:|---|---|
| `4 GiB` | Impossible-math and fail-closed/offload discovery tier. | `rejected` or `measured-offload`; no resident or deployable claim. |
| `6 GiB` | Stretch constrained tier. | `decode-smoke`, `quality-gated`, `measured-offload`, or `rejected`. |
| `8 GiB` | Primary constrained 9B target. | `quality-gated`, `profitable`, `validated-benchmark`, or `rejected`. |
| `12 GiB` | Primary validation tier when 8 GiB is too tight. | `quality-gated`, `profitable`, `validated-benchmark`, or `rejected`. |
| `16 GiB` | Current ceiling/reference tier. | `validated-benchmark` or `rejected`; a pass here does not promote lower tiers. |

Each tier is `requested_tier_bytes`. Its executable bound remains the lower
T-100 `effective_live_cap_bytes`; the tier label never overrides physical VRAM,
the WDDM budget, CUDA availability, or the nonzero reserve.

Acceptance thresholds for `validated-benchmark`:

- `peak_vram <= effective_live_cap_bytes <= requested_tier_bytes`, with
  `requested_tier_bytes <= policy_ceiling_bytes = 17179869184`
- accepted C4 supervisor clearance with pre-context, post-context,
  model/backend reconciliation, watchdog, cancellation, cleanup, and lease
  evidence
- no unreconciled backend, workspace, retained-pool, KV, or unknown allocation
  bytes
- quality fixture pass rate `>= 95%` against the retained prompt suite
- no task-quality regression greater than `5%` versus same-model same-quant
  llama.cpp/GGML reference for the declared fixture
- sustained warm-cache decode `p50 >= 3 tokens/sec`
- `p95` inter-token latency `<= 750 ms`
- warm-cache TTFT `p95 <= 30 seconds`
- three retained repeat runs with sustained decode coefficient of variation
  `<= 10%`
- host memory and IO telemetry retained whenever CPU/offload participates
- no full FP16 weight materialization in VRAM
- exact mixed-quant tensor inventory and byte histogram reconcile with the GGUF
  artifact hash and resident weight ledger
- compression-enabled runs record effective bits, metadata overhead,
  reconstruction workspace, attention error, top-k overlap, and quality deltas
  for the exact cell

Reject the 9B cell when any of these occur:

- requested GPU cap exceeds `17179869184` bytes
- requested tier is substituted for an unavailable or smaller effective live
  cap, or the nonzero reserve is counted as payload
- P6-04A/#103 or the required C4 clearance evidence is missing
- pre-context/post-context admission, watchdog, cancellation, or cleanup
  evidence is missing, stale, contradictory, or failed
- exact model or quantization hashes are missing
- a nominal `q4`/`Q4_K_M` label is used without the exact tensor quantization
  inventory, recipe identity, payload bytes, and metadata bytes
- full FP16 weights are materialized in VRAM
- nominal compression bit width is reported without effective bits, metadata
  overhead, reconstruction cost, and quality evidence
- quality fixture evidence is absent or below threshold
- performance is measured only as an isolated kernel timing
- offload hides unbounded host RAM, pagefile, mmap, or NVMe pressure
- a result from another model, quantization, context, backend, OS, GPU, driver,
  CUDA version, or cap tier is used as evidence

