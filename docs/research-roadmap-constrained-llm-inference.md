# Research Roadmap: Constrained LLM Inference

Program authority: repository-root [`Plan.md`](../Plan.md). This document
retains the detailed Phase 1-6 research history; `Plan.md` controls current
thesis, dependency order, clearance, status, and Project #2 synchronization.

This roadmap defines how PrismInfer should move from Phase 0 telemetry to real
low-VRAM inference research.

## Research Question

Can PrismInfer make large-model inference governed, measurable, fail-closed, and
useful under constrained GPU memory by first building a safe evidence/control
substrate, then testing optional mechanisms independently?

The active core combines:

- llama.cpp/GGML/GGUF compatibility,
- hard-cap memory governance,
- quantized weights,
- CPU/GPU/NVMe offload,
- strict lifecycle and manifest evidence,
- task-level quality gates,
- an exclusive lease, contained worker, two-stage admission and watchdog,
- one conservative static exact-cell plan over implemented actuators,
- manifest-backed 9B evidence construction before any promoted kernel claim,
- exact per-tensor GGML quant semantics for the pinned GGUF recipe.

Paged/quantized KV, new kernels, staging, speculation, progressive artifacts,
and structured compute are independent optional hypotheses. None is required
for the safety substrate or static-controller result.

## Non-Question

The question is not whether dense 90B weights fully fit under the current
16 GiB maximum GPU cap. They do not without an unproven compression/offload
claim that must be separately validated.

The 90B question is whether a hybrid/offload profile can be bounded, repeatable,
and useful enough to earn a validated-benchmark or deployable-profile label.

## Validation Envelope and Matrix

The roadmap is governed by `docs/validation-matrix.md`.

Current maximum GPU cap:

```text
16 GiB = 17179869184 bytes
```

Active VRAM tiers:

```text
1 GiB, 2 GiB, 4 GiB, 6 GiB, 8 GiB, 12 GiB, 16 GiB
```

Model buckets start at `<=2B`, then `>2B-5B`, then 5B-wide bands through
`>85B-90B`.

Every promoted result must bind to a validation cell containing exact model
hash, quantization, context length, VRAM tier, backend, OS, hardware class, and
claim status. No pass generalizes across buckets or tiers by analogy.

The resident GPU admission check is:

```text
peak_vram =
  resident_weight_bytes
+ resident_kv_bytes
+ activation_workspace_bytes
+ cuda_context_runtime_bytes
+ allocator_fragmentation_bytes
+ telemetry_safety_margin_bytes

certify only if peak_vram <= hard_vram_cap_bytes
```

## Phase 1: Backend Governance

Goal: make memory truthful for real llama.cpp-backed inference.

Decision question:

Can PrismInfer govern real backend allocation behavior, not only its own
Phase 0 allocation tracker?

Required work:

- Enforce the current maximum cap policy of `17179869184` bytes.
- Add validation-cell fields to config, telemetry, and manifests.
- Add a backend adapter boundary.
- Add `NullBackendAdapter` for current behavior.
- Add fake backend tests for deterministic success/failure cases.
- Pin llama.cpp/GGML/GGUF commits and model artifacts before real backend runs.
- Introduce `LlamaBackendAdapter` behind an opt-in build flag.
- Replace placeholder `backend_warmup` with real backend warmup evidence.
- Emit backend lifecycle events and manifest fields.
- Detect or bound hidden backend allocations.
- Capture minimum host-memory and IO telemetry for real backend claims.
- Capture minimal KV metadata/profile for backend warmup, while leaving detailed
  KV ledger/compression work to Phase 2.

Required evidence:

- JSONL telemetry.
- Benchmark manifest.
- Lifecycle validator pass.
- Validation cell id and status.
- Dependency pin record.
- Model hash and sidecar hash.
- Backend warmup peak.
- Host-memory and IO telemetry for real backend claims.
- Cap certification or precise fail-closed reason.

Exit claim:

PrismInfer can govern a pinned llama.cpp-backed warmup/profile under the declared
cap, or it fails closed.

## Phase 2: KV Cache and Compression Research

Goal: reduce runtime memory pressure without losing useful model behavior.

Decision question:

Can KV compression or placement reduce memory materially while preserving
task-level quality?

Research lanes:

- KV block accounting inspired by PagedAttention.
- KIVI/KVQuant-style asymmetric KV quantization.
- PolarQuant/TurboQuant/QJL-style dot-product-preserving vector compression.
- CPU KV offload and promotion.
- Prefix reuse and cache isolation policy.

Required measurements:

- Validation cell id, model hash, quantization, context, backend, OS, and VRAM
  tier.
- KV bytes by layer, head, token, context length, and placement.
- Payload bits and metadata bits per value.
- Effective bits per value.
- Attention-logit error distribution.
- Attention top-k overlap.
- Perplexity delta.
- Needle retrieval accuracy.
- Long-context task score deltas.
- Time-to-first-token and tokens/sec.

Exit claim:

Specific compression or placement profiles are quality-gated and memory-gated
for specific validation cells. A pass in one model bucket, VRAM tier, or
hardware class does not promote another cell.

## Phase 3: GPU and Offload Profitability

Goal: determine when bounded GPU use is actually worth using.

Decision question:

Does GPU or CPU/NVMe offload improve end-to-end performance after transfer,
warmup, context, and memory costs are counted?

Required measurements:

- CUDA context overhead.
- Windows driver mode and TDR assumptions where applicable.
- Host memory, pinned host memory, staging buffers, and IO telemetry.
- H2D and D2H bytes per token.
- NVMe bytes per token.
- Page faults or residency proxy.
- Copy wait time.
- Kernel time versus transfer time.
- CPU-only baseline.
- GPU-probed baseline.
- Cold-cache and warm-cache runs.

Exit claim:

GPU/offload mode is profitable only when it beats CPU-only by a declared margin
on end-to-end metrics within the same validation cell while remaining
cap-certified.

## Phase 4: Large-Model and 90B Hybrid Validation

Goal: move large-model profiles, including the terminal 90B bucket, from
simulated to validated only if evidence supports it.

Decision question:

Can a large-model hybrid/offload profile produce useful tokens under a declared
hardware, memory, context, latency, and <=16 GiB GPU-cap envelope?

Required profile fields:

- exact model id and model hash,
- parameter count and validation bucket,
- quantization format and artifact hash,
- context length,
- batch size,
- GPU hardware and driver mode,
- CPU RAM and NVMe profile,
- max GPU cap,
- validation cell id,
- host memory peak,
- IO bytes/token,
- time-to-first-token,
- sustained tokens/sec,
- quality gate results,
- failure artifacts for rejected runs.

Exit claim:

Only validated-benchmark or deployable-profile labels can imply real large-model
or 90B inference. Simulated and measured-offload labels cannot.

## Phase 5: Measured Compute Kernel Research

Goal: implement and evaluate one PrismInfer-specific CUDA kernel prototype after
llama.cpp/GGML baselines, vendor-library baselines, runtime telemetry, 9B
validation-cell declaration, and allocation evidence are strong enough to make
fair comparisons.

Decision question:

Can a narrow kernel path improve end-to-end decode or prefill performance for an
exact validation cell without violating the <=16 GiB cap, hiding dequantization
or launch overhead, bypassing quality gates, or making claims that llama.cpp,
cuBLASLt, CUTLASS, or GGML already satisfy?

Required prerequisites before the custom CUDA kernel prototype:

- strict kernel benchmark and telemetry schemas that reject unknown kernel
  fields,
- same-cell benchmark comparator across model hash, quantization, context,
  prompt fixture, backend, OS, GPU, driver, CUDA version, and cap tier,
- real quality fixtures with retained hashes,
- Windows driver mode, TDR, and WDAC/Application Control evidence where
  applicable,
- llama.cpp/backend allocation reconciliation, or an explicit
  `measured-non-certified` label,
- CPU reference and llama.cpp/GGML CUDA/MMQ baselines for the same cell,
- a pinned 9B-class `>5B-10B` q4 GGUF validation cell before prototype
  promotion.

Initial implementation and research lanes:

- one guarded CUDA implementation of fused q4 dequantization plus matmul for
  batch-1 decode GEMV,
- GEMM versus GEMV dispatch policy for decode and prefill,
- cuBLASLt, CUTLASS, and Tensor Core baseline strategy before custom kernels,
- FlashAttention-style IO-aware attention only after matmul evidence,
- MLA-style latent KV only as a later KV/attention lane,
- low-rank/SVD and structured sparsity first as accounting and artifact
  metadata, not runtime compression,
- MoE active-parameter accounting before any MoE runtime path.

Exit claim:

Phase 5 may keep one measured kernel prototype only when it is correct
against a CPU reference, cap-accounted or explicitly non-certified, compared
against same-cell llama.cpp/GGML/vendor baselines, and at least 10% faster on
end-to-end decode for the declared cell. Otherwise the kernel lane remains
research-only or is rejected.

## Phase 6: Safety and Exact Evidence Foundation

Goal: implement #103 fail-closed supervision/staged admission and convert the
existing scaffolding into retained, manifest-backed evidence for one exact
foundation validation cell. Kernel and offline KV results remain optional.

Decision question:

Can PrismInfer safely admit and validate one exact q4 GGUF foundation cell
under an effective live GPU cap, using strict manifests, same-cell baselines,
per-tensor quant truth, mandatory quality fixtures, truthful memory accounting,
and end-to-end decode measurements?

Required work:

- manifest-file ingestion for strict kernel benchmark manifests,
- comparator separation between validation-cell identity and implementation
  variant,
- schema validation for the 9B kernel gate config,
- compression manifest fields for algorithm family, scope, effective bits,
  metadata overhead, residual/sketch policy, and reconstruction workspace,
- compression-aware memory ledger fields for resident q4 weights, KV payload,
  KV metadata, residual windows, rotations/projections, dequant workspace,
  reconstruction workspace, retained pools, and unknown bytes,
- self-hosted CUDA kernel workflow,
- guarded CUDA launch correctness test,
- exact selected GGUF q4 block-reference semantics,
- optional offline KV compression evaluator for KIVI/KVQuant/QServe-style
  policies and PolarQuant/TurboQuant/QJL reference experiments,
- 9B quality fixture runner with deterministic prompts, retrieval/needle, and
  long-context fixtures,
- kernel benchmark runner that emits strict manifests,
- retained CPU/no-custom and llama.cpp/GGML CUDA/MMQ baselines,
- retained PrismInfer candidate kernel evidence only when that optional lane is
  attempted,
- result classification as `research-only`, `measured-non-certified`,
  `quality-gated`, `validated-benchmark`, or `rejected`.

Compression workflow:

```text
pinned 9B GGUF artifact
  -> q4 resident weight plan
  -> no full FP16 materialization gate
  -> fused/tiled q4 dequant and decode GEMV
  -> KV ledger
  -> optional governed KV compression policy
  -> reconstruct/dequant hot path
  -> memory ledger sample
  -> quality and performance gates
  -> same-cell comparator classification
```

Compression policies are staged:

1. q4 resident weights with no runtime compression novelty.
2. KV accounting-only.
3. KIVI/KVQuant/QServe-style KV compression.
4. PolarQuant/TurboQuant/QJL offline/reference experiments.
5. Hot-path PolarQuant/TurboQuant/QJL only after exact-cell quality and
   overhead evidence exists.

Exit claim:

The 9B representative cell is either `validated-benchmark`, `quality-gated`,
`measured-non-certified`, `rejected`, or remains `research-only`. A pass does
not imply deployability and does not generalize to the whole `>5B-10B` bucket.

## Proposed Phase 7-9 Program: Calibrated Adaptive Runtime

The detailed program lives in `docs/adaptive-runtime-v2/`. The root
`Plan.md` packet table is the only execution order: Packet A remains
CPU/offline, Packet B closes the hardware boundary, and Packet C requires both
before model-backed work. No prose in this roadmap grants clearance.

The binding sequence is:

1. **Phase 7 - secure static foundation.** Replace the shell execution boundary,
   establish Windows/WDDM and file-residency evidence, inventory the pinned
   llama.cpp/GGML actuators, admit scale cells with resource-DAG lower bounds,
   and run the exact admitted Llama 3.1 8B foundation experiment. A
   PrismInfer plan is executable only when every decision maps to an
   acknowledged actuator and an explicit `R0` through `R3` recovery class.
2. **Phase 8 - optional mechanisms.** Establish a 30B static placement result
   first. Kernel selection, bounded staging, architecture-state compression,
   speculative decoding, and structured compute remain independent research
   providers. Each must beat the static baseline and pass its own quality,
   memory, security, and tail-latency gates before joint optimization.
3. **Phase 9 - gated scale and portability.** Activate 70B or 90B experiments
   only when Phase 7 capacity and bandwidth admission says the exact cell can
   meet the declared service envelope. Recalibration and invalidation rules are
   tested across hardware and software fingerprints before a final audit.

The preferred foundation is a self-produced, hash-pinned Llama 3.1 8B Instruct
text GGUF, subject to accepted license/access. Ornith/Qwen3.5-class models are
separate hybrid stress cells because their
DeltaNet/full-attention architecture carries recurrent state that cannot be
modeled as uniform KV cache. Optimizer throughput is measured in committed,
target-distributed output tokens per second; speculative acceptance rate is a
diagnostic rather than the objective.

## Claim Taxonomy

| Claim label | Evidence requirement |
|---|---|
| `proven` | Implemented, tested, and CI/local verified. |
| `bounded` | Cap-governed with manifest-backed evidence but limited scope. |
| `quality-gated` | Memory and task-quality gates both pass. |
| `profitable` | Transfer-inclusive performance beats baseline. |
| `simulated` | Planner or lower-bound result only. |
| `measured-offload` | Real execution with offload evidence but not yet deployable. |
| `rejected` | Evidence fails a declared gate. |

## Tiered Validation Ladder

The first targets after Phase 0 are representative cells, not broad model-size
claims:

1. `<=2B` under 1 GiB and 2 GiB caps: real backend warmup and decode smoke.
2. `>2B-5B` under 1 GiB, 2 GiB, and 4 GiB caps: real backend warmup and decode
   smoke.
3. `>5B-10B`: run the exact Llama 3.1 8B foundation first, then Ornith
   9B only as a separately admitted hybrid-state stress cell. Requested 10 GiB
   and 12 GiB are the primary constrained tiers, 8 GiB is stress-only, and the
   physical/live device reference is separate. Context, batch, artifact,
   quality, and statistical gates are frozen by Adaptive Runtime V2 rather than
   duplicated here.
4. `>10B-15B`: quality-gated constrained inference by exact validation cell.
5. Larger buckets through `>85B-90B`: simulated, measured-offload, validated, or
   rejected according to retained evidence.
6. Compute kernel research: same-cell measured only, never generalized across
   model buckets, quantization formats, batch sizes, GPU architectures, or cap
   tiers by analogy.
7. Phase 6 evidence construction: exact 9B cell artifacts and manifests must be
   retained before any kernel performance claim is promoted.
8. Proposed Phase 7 controller proof: the exact admitted Llama 3.1 8B
   cell must pass actuator, recovery, memory, quality, and tail-latency gates
   before dynamic mechanisms or larger-model conclusions are activated.

No cell above 16 GiB is in scope for the current roadmap.
