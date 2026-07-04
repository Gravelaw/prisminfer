# Validation Matrix

This document is the canonical validation envelope for PrismInfer model-size and
VRAM-tier claims.

## Current Envelope

Current maximum GPU hard cap:

```text
16 GiB = 17179869184 bytes
```

No roadmap item, config, benchmark claim, or GitHub issue should require or
validate a GPU cap above 16 GiB until the cap policy is explicitly changed.

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

certify only if peak_vram <= hard_vram_cap_bytes
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
  + quantization_format
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
context_tokens
vram_tier_gib
hard_vram_cap_bytes
validation_cell_id
validation_cell_status
```

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
- dependency pin record,
- model hash and quantization artifact hash,
- backend and OS/hardware profile,
- cap certification or precise fail-closed reason,
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
- quantization: pinned GGUF q4-family artifact, preferably fixed-block
  `Q4_K_M` or the exact q4 format used by the selected model
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

Acceptance thresholds for `validated-benchmark`:

- `peak_vram <= hard_vram_cap_bytes`, with
  `hard_vram_cap_bytes <= 17179869184`
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
- compression-enabled runs record effective bits, metadata overhead,
  reconstruction workspace, attention error, top-k overlap, and quality deltas
  for the exact cell

Reject the 9B cell when any of these occur:

- requested GPU cap exceeds `17179869184` bytes
- exact model or quantization hashes are missing
- full FP16 weights are materialized in VRAM
- nominal compression bit width is reported without effective bits, metadata
  overhead, reconstruction cost, and quality evidence
- quality fixture evidence is absent or below threshold
- performance is measured only as an isolated kernel timing
- offload hides unbounded host RAM, pagefile, mmap, or NVMe pressure
- a result from another model, quantization, context, backend, OS, GPU, driver,
  CUDA version, or cap tier is used as evidence

