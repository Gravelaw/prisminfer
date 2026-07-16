# Upstream, Closest-Work, and Novelty Gap Matrix

Status: required before any PrismInfer novelty claim.

This matrix distinguishes an engineering integration contribution, an
evidence/governance contribution, and a new mechanism/result. Combining known
variables in one optimizer tuple is not automatically novel.

## Canonical Thesis Boundary

The core PrismInfer contribution under test is a safety-supervised,
offline-calibrated **static** controller over pinned llama.cpp/GGML/GGUF. It
selects only implemented, acknowledged controls; emits an immutable plan; and
replays or rejects it under measured GPU/host-memory and compatibility guards.

Custom kernel dispatch, KV compression, progressive representations,
speculation, and prompt/hidden-state routing are independent, optional
hypotheses. None is a Phase 6/7 completion dependency, and combining them does
not create novelty by itself.

Legend:

- `Yes`: primary capability of the referenced system/source.
- `Partial`: related capability with a different scope or contract.
- `No/unknown`: not established by the reviewed source.
- PrismInfer gap: the exact question the project must implement or measure.

## Capability Matrix

| Capability | Pinned/current llama.cpp | FlexGen | PowerInfer/2 | HeteGen/APEX | SpecOffload/SubSpec | Any-Precision/BitStack | DejaVu | PrismInfer gap to test |
|---|---|---|---|---|---|---|---|---|
| GGUF/GGML native Windows baseline | Yes | No | Different runtime/model formats | Different runtime | Different runtime | Different serving formats | Different runtime | Secure, pinned, evidence-correct control over the existing GGUF ecosystem. |
| Fit model/context to device memory | Current upstream yes; pin-dependent | Placement optimizer | Partial placement | Partial | Planner includes placement | No | No | Exact pinned actuator inventory plus requested/actual cap evidence. |
| Tensor/layer CPU-GPU placement | Yes, mostly load/static controls | Yes, GPU/CPU/disk | Yes, activation/neuron-aware | Yes | Yes | No | Calibrated static selection on this Windows laptop; dynamic only with real actuator. |
| Shape-specific internal kernel dispatch | GGML internal; no stable external per-op selector | No central contribution | Specialized kernels | System-specific | System-specific | Specialized serving | Specialized sparse kernels | Optional: trace actual path and test a narrow default-safe hook only if a measurable gap exists. |
| Offline hardware calibration/autotuning | Partial fit/bench tools | Cost model/optimization | Profiling/planning | Profiling-informed | Planner | Format-specific | Predictor/runtime | Unified raw evidence, uncertainty, plan key, acknowledgement, and abstention for exact GGUF cells. |
| Hard 16 GiB cap with fail-closed evidence | No PrismInfer contract | Capacity in optimizer | System-specific | System-specific | System-specific | No | No | Owned-allocation versus WDDM physical-residency claim with host/file/pagefile evidence. |
| Immutable plan with requested/actual acknowledgement | CLI/config, limited actual-path contract | Optimized schedule | Plan/runtime | Schedule/runtime | Plan/runtime | Format/runtime | Predictor/runtime | Versioned actuator-bound plan, explicit recovery class, and exact-cell evidence. |
| KV quantization/types | Current upstream types; pin-dependent | Not core | Partial | Partial | Not core | No | No | Optional: compare one approved candidate with architecture-state accounting and quality/fallback; no win is required. |
| Dynamic heterogeneous KV placement | Partial offload | Placement principles | Partial | Partial | KV effects | No | No | Separate full-attention KV, hybrid recurrent/convolution state, WDDM/host tiers, and exact conversion contracts. |
| Speculative decoding | Current upstream multiple modes | No | No | No | Yes, coupled to offload | No | No | Optional: compare upstream first; optimize committed target-distributed output tokens and observed external bytes. |
| Progressive/nested weight precision | No general nested GGUF | No | No | No | Substitute/low-bit draft differs | Yes | No | Optional static derived format plus exact decoder/kernel/provenance; dynamic tiers only after an isolated win. |
| Prompt/task plan prior | No joint planner | Workload batch planning | Activation predictors | Profiling | Acceptance/planner state | No | Contextual hidden state | Non-semantic warm-start/prefetch only. |
| Hidden-state structured compute | No generic dense path | No | Yes | No | No | No | Yes | Optional hardware-aligned oracle on foundation and Ornith cells; router/adaptation only if opportunity survives overhead/quality. |
| End-to-end claim taxonomy and retained evidence | Benchmark tools but no PrismInfer policy | Research evaluation | Research evaluation | Research evaluation | Research evaluation | Research evaluation | Research evaluation | Exact-cell cap/quality/provenance classification, negative results, and reproducibility bundle. |
| 30B-90B artifact-specific early rejection | Fit/load behavior, no PrismInfer research gate | Large-model scheduling | Larger/sparse models | Evaluated models | Evaluated offload | Compression formats | Large-model contextual sparsity | Native Windows exact-artifact capacity/DAG lower bounds and measured-or-rejected cells. |

## Closest-Work Boundaries

### llama.cpp/GGML

Already provides much of the executable substrate: model loading, quantized CPU
and GPU paths, fit behavior, GPU layers/tensor overrides, KV types, mmap/direct
IO, and speculative modes in current upstream. PrismInfer must not claim these
features.

Potential contribution:

- exact pin/actuator inventory;
- native Windows evidence and recovery contract;
- calibration across public controls with requested/actual acknowledgement;
- optional narrow hook if current dispatch cannot be controlled externally.

### FlexGen

Already formulates placement/scheduling across GPU/CPU/disk for large-model
throughput. PrismInfer is not the first heterogeneous placement optimizer.

Potential difference to test:

- low-batch consumer Windows/GGUF target;
- exact cap and WDDM/host/file evidence;
- bounded static controller first;
- different latency/accepted-output objective.

### PowerInfer and PowerInfer-2

Already combine activation locality, prediction, heterogeneous devices, neuron
clusters, caching, and scheduling. PrismInfer is not the first conditional
CPU/GPU inference runtime.

Potential difference to test:

- whether an unmodified conventional/Ornith model has profitable
  hardware-aligned opportunity under GGML;
- strict oracle stop gate and explicit approximate/pre-commit contract;
- integration into the same cap/reproducibility framework.

### HeteGen and APEX

Already use profiling-informed CPU/GPU overlap. PrismInfer is not the first
profiling-based heterogeneous scheduler.

Potential difference to test:

- native Windows hybrid-core/WDDM target;
- actuator-bound plan and resource DAG;
- exact GGUF cell and negative/rejection outcomes.

### SpecOffload and SubSpec

Already couple speculation, placement, target transfer, and acceptance.
PrismInfer is not the first speculative offload planner.

Potential difference to test:

- current llama.cpp speculation baseline;
- committed target-distributed output tokens and observed external bytes;
- integration with exact cap/recovery/host evidence on the target machine.

### Any-Precision and BitStack

Already provide progressive/nested representation concepts with specialized
serving formats. PrismInfer is not the first progressive-precision runtime.

Potential difference to test:

- an optional derived artifact compatible with the PrismInfer evidence and
  provider contracts;
- fair comparison with best same-size ordinary GGUF quant on the exact cell.

### DejaVu

Already demonstrates hidden-state contextual sparsity with trained predictors
and specialized execution. PrismInfer is not the first contextual-sparsity
system.

Potential difference to test:

- structured opportunity in selected modern models;
- prompt clustering only as a prior;
- explicit pre-commit or lossy contract, OOD, dense audits, and fallback
  classification.

## Model and Quantization Naming Boundary

- P7-01 owns the exact model decision. Meta Llama 3.1 8B is the preferred
  conventional foundation pending license acceptance, access, revision,
  converter support, and reproducible hashes.
- Ornith-1.0-9B is a separate hybrid capability/stress cell; it cannot stand in
  for the ordinary dense-attention foundation.
- Gemma 2 may be an optional comparison, but its interleaved
  sliding-window/global-attention architecture is not labeled globally
  full-attention.
- `Q4_K_M` names a mixed quantization recipe, not one tensor block encoding.
  Evidence and exact-path claims bind the recipe plus each tensor's actual
  `ggml_type`, layout, shape, and bytes.

## Current Implementation Boundary

Manifest ingestion, same-cell comparison scaffolding, Phase 6 schemas, and a
guarded synthetic CUDA `Q4Block` correctness lane exist. Exact model tensor
semantics, a model-backed baseline, the offline static selector, and executable
acknowledged plans do not. [#103](https://github.com/Gravelaw/prisminfer/issues/103)
blocks model-backed Phase 6 work until the hardware supervisor and pre-context
admission boundary pass.

## Allowed Contribution Claims

### C1: engineering integration

Example form:

> PrismInfer implements an actuator-bound calibrated controller over pinned
> llama.cpp/GGML/GGUF on native Windows for the declared exact cell.

Requires implementation, tests, and exact scope. It does not claim a new
optimization algorithm.

### C2: evidence/governance methodology

Example form:

> PrismInfer distinguishes owned allocation from WDDM physical residency,
> observes host/file/pagefile/transfer evidence, and classifies exact-cell
> results with explicit recovery and reproducibility contracts.

Requires a demonstrated gap versus existing tooling and retained evidence.

### C3: new policy/mechanism/result

Example form:

> On exact cell X, policy Y improves committed output throughput or another
> frozen metric over the strongest composed baseline while preserving cap and
> quality gates.

Requires:

- closest composed baseline;
- frozen threshold/sampling plan;
- exact requested/actual path;
- confirmatory result and uncertainty;
- mechanism explanation beyond placing known variables in one score.

## Prohibited Claims Until Evidence

- First heterogeneous LLM planner.
- First dynamic low-VRAM runtime.
- Novel merely because kernel, placement, KV, compression, and speculation are
  listed jointly.
- Model-bucket or hardware-family generalization from one cell.
- Dynamic/conditional behavior when only load-time static controls exist.
- Interactive 70B/90B before lower-bound admission and measured evidence.

## Novelty Decision Gate

P7-01 selects the model cells; P7-04 inventories the selected pinned/current
upstream actuators and updates this matrix. #103 must close before model-backed
evidence collection. P7-10 may claim C1/C2 only for implemented evidence.
Optional Phase 8 work may claim C3 only after the strongest relevant composed
baseline and the versioned threshold registry pass.
