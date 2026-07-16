# Adaptive Runtime Scope and Requirements

Status: final theory baseline for the implementation pass. Phase 6 scaffolding
is partially implemented; model-backed Phase 6 work remains blocked by the
runtime-supervisor and admission-boundary gate in
[#103](https://github.com/Gravelaw/prisminfer/issues/103).

Council run: 2026-07-10 to 2026-07-11.

## Decision Summary

PrismInfer will be developed first as a **safety-supervised, offline-calibrated,
static heterogeneous execution controller** over a pinned
llama.cpp/GGML/GGUF substrate. It will not begin as a clean-sheet model runtime,
an online optimizer, or a collection of mandatory custom mechanisms.

PrismInfer owns:

- hardware, runtime, model, and workload fingerprints;
- calibration experiments and retained raw telemetry;
- constrained candidate search and plan selection;
- versioned execution-plan sidecars and compatibility checks;
- cap admission, evidence, quality gates, actuator-specific recovery, and drift
  detection;
- translation of a plan into supported llama.cpp/GGML controls; and
- later, narrowly scoped scheduling or operator hooks that pass an explicit,
  independent integration gate and are not required for the core controller.

llama.cpp and GGML continue to own:

- GGUF loading and exact tensor semantics;
- the model graph, tokenizer, sampling, and ordinary inference lifecycle;
- baseline CPU and GPU kernels;
- baseline KV implementation and supported KV types;
- backend buffers and scheduling until a narrow, versioned integration is
  justified; and
- the default safe execution path.

The program is technically ambitious but claim-conservative. Development cost
is deliberately excluded from prioritization, as requested. Sequencing is based
on correctness, dependency, measurement quality, and research value.

## Problem Statement

The target problem is not simply to load a large quantized file. It is to
produce useful tokens from an exact 8B/9B, 30B, 70B, or 90B-class artifact while
governing:

- a maximum GPU cap of 16 GiB;
- actual host memory and commit limits;
- quantized weight and metadata bytes;
- KV growth and workspace;
- CPU and GPU execution time;
- PCIe and storage traffic;
- cold and warm behavior;
- task-level quality;
- TTFT, inter-token latency, and throughput; and
- uncertainty or drift between calibration and execution.

For a dense model, quantization reduces bytes but does not remove the need to
evaluate most active weights. A plan that technically loads a model through
mmap, pagefile, or NVMe is not thereby useful or interactive.

## Program Thesis

> PrismInfer is a safety-supervised execution-planning layer over
> llama.cpp/GGML/GGUF. It calibrates offline, selects only implemented static
> llama.cpp/GGML controls, emits an immutable acknowledged plan, and replays or
> rejects that plan under hard GPU/host-memory, compatibility, latency, and
> quality envelopes.

Custom kernel dispatch, KV compression, progressive representations,
speculation, prompt clustering, and hidden-state routing are independent,
nonblocking research hypotheses. They may be measured only after the static
controller works, and no Phase 6 or Phase 7 exit depends on any of them winning.
The word *conditional* therefore describes a later optional lane, not the core
runtime thesis.

## Relationship to Existing Phases

The adaptive program extends rather than replaces the current roadmap.

| Existing phase | Required contribution to adaptive work |
|---|---|
| Phase 0 | Hard-cap semantics, allocator/process/device telemetry, lifecycle validation, and fail-closed behavior. |
| Phase 1 | Pinned llama.cpp/GGML/GGUF boundary and backend warm-up evidence. |
| Phase 2 | KV accounting, compression quality, and exact-cell quality gates. |
| Phase 3 | Transfer-inclusive offload profitability and host/IO evidence. |
| Phase 4 | Large-model claim taxonomy and simulated versus validated distinction. |
| Phase 5 | Kernel evidence rules and the one-kernel continuation boundary. |
| Phase 6 | Exact selected foundation artifact, mixed-recipe/per-tensor quant semantics where applicable, manifests, baselines, fixtures, and optional mechanism evidence. |
| Phase 7+ | Offline calibration, static plan replay, acknowledgement/recovery, and only later optional phase-aware or conditional mechanisms. |

Architecture and documentation work may proceed in parallel with Phase 6.
Model-backed CUDA, calibration, or evidence collection may not start until
[#103](https://github.com/Gravelaw/prisminfer/issues/103) closes its
runtime-supervisor and pre-context admission boundary. Automatic plan replay
may not promote a claim until its Phase 6 entry gates are satisfied.

## Current Repository Baseline

The council reviewed the repository as it exists, not as the roadmap describes
it aspirationally.

What exists:

- cap, lifecycle, manifest, quality, and claim-policy scaffolding;
- Windows host process telemetry;
- a static hybrid-plan byte ledger;
- configured offload and transfer evidence fields;
- a pinned llama.cpp commit record;
- a process-backed llama adapter;
- strict kernel-manifest file ingestion and same-cell comparison scaffolding;
- Phase 6 gate/config schemas and compression-evidence fields; and
- a guarded synthetic CUDA q4 launch/correctness lane using toy `Q4Block`
  semantics only.

What does not yet exist:

- in-process ownership of a llama context or GGML graph;
- measured tensor transfers or a real transfer scheduler;
- child-process peak host telemetry for the external llama process;
- an operator/shape/actual-kernel trace;
- a calibration database or cost model;
- executable tensor-granular placement plans;
- exact model GGUF q4 CUDA semantics in the candidate kernel;
- the fail-closed hardware supervisor and pre-context admission boundary owned
  by #103;
- prompt/hidden-state routing; or
- a real 30B to 90B adaptive inference profile.

The current `LlamaBackendAdapter` launches a child executable and parses logs.
The offload planner validates caller-provided values, and its configured pinned
and staging budgets are not observed peaks. The simulated 90B plan assumes
128 GiB of host RAM, while the current machine has approximately 32 GiB. These
facts are binding constraints on the next implementation stage.

## Terms

| Term | Meaning in PrismInfer |
|---|---|
| Validation cell | Exact model/quant/prompt/context/batch/backend/hardware/software/cap identity for a result. |
| Hardware fingerprint | Versioned identity and capabilities of CPU topology, RAM, GPU, PCIe, storage, OS, driver, toolkit, and power/thermal envelope. |
| Calibration | Explicit measurement run that produces raw trials, cost estimates, uncertainty, and candidate eligibility. |
| Execution plan | Immutable sidecar containing only implemented actuator decisions, acknowledgements, lifecycle, and recovery classes for one compatibility predicate. |
| Runtime state | Observed phase/state. It is a switching point only when the actuator matrix proves compatible state or declares restart/reject. |
| Derived artifact | Optional packed, quantized, progressive, or indexed representation generated from an immutable source model and bound by hashes and recipe. |
| Conditional compute | Approximate or trained selection of structured blocks, heads, experts, or depths based on request/hidden state. |
| Promoted result | A result allowed beyond `research-only` under the repository claim taxonomy. |

## Ownership and Integration Boundary

| Capability | llama.cpp/GGML | PrismInfer Phase 7 | Later gated extension |
|---|---:|---:|---:|
| GGUF parse and exact quant blocks | Own | Validate/pin | No replacement planned |
| Tokenizer, model graph, sampling | Own | Configure/observe | No replacement planned |
| CPU/GPU baseline kernels | Own | Observe and compare | Optionally register one exact candidate when independently justified |
| GPU layers, tensor buffer type, KV/state type, context/batch | Expose controls | Select only when pinned actuator is proven | Most are load/context-time; switching requires an explicit compatibility contract |
| Actual operator/kernel telemetry | Internal/partial | Require through embedded adapter | Narrow upstreamable hook |
| Hardware fingerprint and plan key | No joint contract | Own | Extend across platforms |
| Cross-resource cost model | Limited local heuristics | Own an offline static selector | Learned ranker only if justified |
| Hard cap and evidence classification | No PrismInfer contract | Own | Continue fail-closed |
| Pinned staging/prefetch | Backend-specific | Not a Phase 7 critical dependency | Optional bounded event-driven scheduler |
| Progressive representation | Not general | Offline research/evidence | Derived artifact and fused path |
| Structured conditional compute | Model-specific | Trace/simulate only | Guarded executor, adaptation if required |

## In Scope

### Phase 7: secure, safety-supervised static calibrated controller

- Preserve the exact source GGUF and bind every derived artifact to it.
- Under P7-01, select and pin Meta Llama 3.1 8B as the preferred conventional
  foundation cell, subject to license acceptance, access, exact revision,
  converter support, and reproducible GGUF hashes. Keep Ornith-1.0-9B as a
  separate hybrid capability/stress cell.
- Replace shell execution with a native Job-backed process boundary and secure
  artifact/plan handling.
- Implement the Windows/WDDM/child/host/file/pagefile/transfer evidence
  protocol.
- Inventory every pinned-runtime actuator, lifecycle, acknowledgement, and
  recovery class before candidate construction.
- Run exact selected-foundation/9B-stress/30B/70B/90B capacity and
  resource-DAG lower bounds early.
- Build an in-process, pinned llama.cpp/GGML adapter.
- Retain the external process adapter as a baseline/fallback.
- Fingerprint CPU topology, RAM/commit, GPU/VRAM, PCIe, storage, OS, driver,
  toolkit, runtime pin, model, and power/thermal bucket.
- Capture actual operator type, tensor type/layout, dimensions, phase, placement,
  transfer, workspace, and invoked implementation where available.
- Calibrate supported llama.cpp controls before proposing new mechanisms.
- Enumerate only proven static/load/context/request actuators: CPU-only,
  GPU-resident, contiguous layer split, proven tensor overrides,
  architecture-state type, thread topology, and batch/ubatch.
- Fit conservative cost models with prediction intervals.
- Select a Pareto-feasible plan subject to cap, quality, and latency gates.
- Emit a versioned, hashed static plan bundle with local, compatible-boundary,
  or restart/reject recovery.
- Replay one acknowledged static plan through the pinned runtime.
- Detect compatibility misses and drift without searching on the token path.
- Validate the full loop first on the conventional full-attention foundation,
  then on the Ornith hybrid stress cell.

Phase 7 succeeds when the controller safely selects, acknowledges, replays, or
rejects the best supported static plan. It does not require a custom-kernel
speedup, a KV-compression win, speculation, progressive weights, or a router.

### Phase 8: 30B static truth and independent optional research

- Measure 30B static contiguous CPU/GPU placement before optional mechanisms.
- Optional kernel hook/autotuning after the static controller.
- Optional bounded staging/prefetch after the 30B static baseline.
- Architecture-state blocks with governed residency/precision only through an
  implemented state conversion/recovery contract.
- Offline evaluation of progressive base-plus-residual representations.
- Split entropy/random-access diagnostics, exact lossless cold cache, static
  progressive weights, and activation transfer compression into independent
  hypotheses. None is a phase-exit dependency.
- Prompt/task clustering only for initial profile selection and prefetch.
- Hidden-state activation traces for structured MLP/head block selection.
- A router simulator that includes selection, gather, fallback, and quality
  cost.
- A hardware-aligned oracle and adaptation decision. Router implementation is a
  new gated sub-issue only if the oracle passes.
- Optional fine-tuning/adaptation as a separate model artifact when training-free
  routing fails.
- Speculative offload that measures committed target-distributed output tokens
  and observed external bytes.
- Joint planning only if at least two independent mechanisms pass; it is not a
  mandatory phase outcome.

Every optional mechanism has its own pass/reject outcome. A negative result
does not block completion of the static-controller program.

### Phase 9: admitted dynamic and large-model validation

- Optional 30B dynamic placement only after the static result.
- Activate 70B and 90B execution only when the Phase 7 artifact-specific
  capacity/resource-DAG lower bound admits them.
- Measured-offload experiments only when the optimistic lower bound meets the
  frozen research threshold.
- Cross-device plan invalidation and recalibration tests.
- Exact classification as validated, quality-gated, measured-offload,
  simulated, rejected, or research-only.

## Deferred but Preserved Research

- AlphaTensor/AlphaEvolve-style automatic algorithm discovery.
- Triton or Mojo kernel prototypes compared against the C++/CUDA baseline.
- Block-scaled/NVFP4 derived artifacts on supported Blackwell paths.
- Virtual-memory-based KV mapping beyond the existing ledger.
- Multi-model or multi-request scheduling.
- Native MoE expert scheduling for a later MoE validation cell.
- Intel iGPU/NPU participation after a real backend and coherence case exists.
- Multi-GPU/NCCL and disaggregated serving.

Deferred means the architecture must not make these impossible; it does not
authorize implementation before the relevant gate.

## Out of Scope

- Replacing llama.cpp/GGML wholesale.
- A general tensor compiler or arbitrary matrix-algorithm synthesizer.
- Editing the source GGUF in place.
- Hiding a full high-precision model materialization behind a compression claim.
- Per-request kernel benchmarking or global optimization in the token loop.
- Unbounded online reinforcement learning on production requests.
- Treating unified memory, pagefile, or WDDM oversubscription as certified
  physical capacity.
- Treating mmap size as resident host memory.
- Treating pinned host memory as a model-residency tier.
- Naive NVMe weight streaming presented as interactive serving.
- Individual-neuron CPU/GPU transfers.
- Full layer skipping on an unadapted dense model without quality evidence.
- Benchmark-score claims copied from a model card as PrismInfer quality results.
- Generalizing one model, quant format, GPU, context, or power state to a broad
  model bucket.
- REST/API product work before the calibrated execution path is valid.
- Development-cost optimization or cost estimates in this roadmap.

## Functional Requirements

### Fingerprinting and identity

- **FR-001:** Produce a stable `hardware_fingerprint_id` from all compatibility
  fields that affect execution.
- **FR-002:** Record exact PrismInfer, llama.cpp/GGML, compiler, CUDA, driver,
  model, tokenizer, quant, prompt, and plan hashes.
- **FR-003:** Separate persistent identity from request-time state such as free
  RAM/VRAM, PCIe link state, temperature, clocks, and power mode.
- **FR-004:** Invalidate or quarantine a plan when a required compatibility field
  changes.

### Calibration

- **FR-010:** Run explicit CPU, GPU, transfer, memory, storage, and end-to-end
  calibration suites.
- **FR-011:** Store raw samples, warm-up policy, trial order, environment state,
  errors, and uncertainty—not only a winning score.
- **FR-012:** Trace real model shapes and tune only eligible high-impact
  candidates.
- **FR-013:** Compare public llama.cpp/GGML controls before adding a fork or
  custom operator.
- **FR-014:** Hold out shapes and prompts to test prediction and selection
  accuracy.

### Planning

- **FR-020:** Generate only decisions whose actuator descriptor, pinned API/hook,
  lifecycle, acknowledgement, memory effect, safe point, and recovery class are
  implemented at the active integration tier.
- **FR-021:** Treat cap, unknown memory, artifact identity, and critical quality
  gates as hard constraints.
- **FR-022:** Compile a resource-constrained dependency-DAG makespan including
  transfer, synchronization, reconstruction, switching, router, draft,
  verification, mandatory target output, and rollback where applicable.
- **FR-023:** Retain the Pareto frontier instead of collapsing every workload to
  one scalar before policy selection.
- **FR-024:** Provide a rejection rationale and an R0 local substitution, R1
  compatible-boundary recovery, or R2 restart/reject action. Approximate R3
  work requires pre-commit verification, checkpointed rollback, or an
  explicitly lossy classification.
- **FR-025:** Produce deterministic output for identical inputs and calibration
  data.

### Plan artifact and execution

- **FR-030:** Emit an immutable, schema-validated execution-plan sidecar.
- **FR-031:** The first plan is static and actuator-bound. Later phase/provider
  plans are distinct only where their lifecycle and state compatibility are
  implemented.
- **FR-032:** Apply a plan through backend acknowledgements and record requested
  versus actual behavior.
- **FR-033:** Use O(1) table lookup or bounded small-state selection in the hot
  path; no calibration there.
- **FR-034:** Change plans only through an actuator-specific R0/R1 boundary;
  otherwise restart/reject and account reload/recompute/quarantine cost.
- **FR-035:** Never claim seamless fallback after model/state/output commit
  without pre-commit verification or a tested checkpoint/rollback contract.

### Telemetry and evidence

- **FR-040:** Distinguish configured budget, predicted value, observed current,
  observed peak, and inferred value in the schema.
- **FR-041:** Reconcile PrismInfer-owned, backend, process, device, WDDM local
  budget/residency, workspace, architecture state, graph, and instrumentation
  memory. Owned-allocation and physical-residency claims remain separate.
- **FR-042:** Measure H2D/D2H bytes, durations, overlap, and uncovered wait.
- **FR-043:** Measure the child/process tree through native handles/Job Object;
  retain working set, private/commit, authoritative system physical and commit
  counters, file-identity-aware model IO, pagefile/ETW evidence or explicit
  ambiguity.
- **FR-043A:** Admit host use through the T-101
  `development_nonpromotable` or `evidence_promotable` lane using separate live
  physical and commit payloads plus exact planned incremental peaks and
  uncertainty. Never impose a fixed free-RAM prerequisite; pagefile capacity
  cannot increase physical payload, and development receipts cannot be
  promoted.
- **FR-044:** Record requested and actual actuator values with acknowledgement,
  lifecycle, recovery class, and reason.
- **FR-045:** Hash and retain profiler artifacts for hardware-path claims.

### Compression and conditional mechanisms

- **FR-050:** Report payload, metadata, alignment, residual, index, and workspace
  bytes as effective bits.
- **FR-051:** Never promote a codec without end-to-end quality and latency.
- **FR-052:** Create lossy or progressive representations offline as distinct
  derived artifacts.
- **FR-053:** Require random access and bounded reconstruction for a hot-path
  representation.
- **FR-054:** Use structured routing units and record router/gather/pre-commit
  verification or explicitly lossy cost.
- **FR-055:** A dense/higher-precision audit after commit is diagnostic only.
  Semantic recovery requires pre-commit verification or a bounded checkpoint,
  output quarantine, rollback, and recomputation contract.
- **FR-056:** Keep prompt classification and hidden-state routing as separate
  mechanisms and evidence.

## Non-Functional Requirements

- **NFR-001 Correctness:** Differential tests against exact GGML/GGUF reference
  semantics are mandatory before performance claims.
- **NFR-002 Safety:** Memory evidence remains fail-closed. Safety reserves are
  workload-relative and lane-specific; an arbitrary 24 GiB-free requirement is
  not a substitute for exact admission.
- **NFR-002A Hardware supervision:** Model-backed work requires pre-context
  admission, an exclusive experiment lease, dispatch deadlines, bounded
  cancellation, live GPU/host pressure supervision, and known-safe cleanup as
  proven by #103.
- **NFR-003 Reproducibility:** Every promoted result satisfies
  `security-privacy-reproducibility.md`, including immutable source revisions,
  auxiliary artifacts, converter/imatrix/splits/randomization, environment,
  dirty patch hash, plan, manifest, and fixtures.
- **NFR-004 Explainability:** Every plan choice, actual acknowledgement, and
  recovery/restart decision has a reason code and supporting measurements.
- **NFR-005 Hot-path overhead:** PrismInfer orchestration must stay within the
  declared overhead budget; the Phase 7 initial gate is at most 2% versus the
  same selected upstream configuration.
- **NFR-006 Stability:** Candidate runs follow the versioned threshold/sample
  registry. Three-run CV is a repeatability diagnostic, not a p95/p99 sample
  plan.
- **NFR-007 Portability:** Unsupported platforms fall back; cross-platform
  abstraction may not erase platform-specific evidence.
- **NFR-008 Upstreamability:** Any llama.cpp/GGML patch is narrow, default-safe,
  versioned, and suitable for upstream proposal.
- **NFR-009 Security and authenticity:** Native launch, approved roots,
  reparse/TOCTOU-safe open-handle identity, bounded decoders, approved provider
  registry, integrity and trust/approval records are mandatory.
- **NFR-010 Privacy:** Fixture-only semantic capture is the default. Production
  prompts/tokens/logits/hidden/KV state require explicit opt-in, purpose,
  access/encryption, retention, and deletion policy.
- **NFR-011 Experiment providers:** Optional CUDA/CUTLASS/Triton/Mojo kernels use
  the versioned no-hidden-allocation C ABI provider contract; none owns the
  runtime boundary.

## Hardware and Model Envelope

### Primary hardware cell

```text
CPU: Intel Core Ultra 9 285H, hybrid 16-core topology
Host RAM: approximately 32 GiB
GPU: NVIDIA GeForce RTX 5080 Laptop GPU
Reported VRAM: 16,303 MiB
GPU cap ceiling: 17,179,869,184 bytes
PCIe: observed Gen 5 x8 link state, subject to calibration
OS: Windows 11 / WDDM
CUDA: pinned toolkit/runtime/driver per manifest
```

Advertised bandwidth and model name are not performance evidence. Laptop power
and thermal state must be retained.

### Model cells

| Cell | Purpose | Status |
|---|---|---|
| Small smoke model | Fast correctness, schema, fault, and CI tests. | Must be selected and pinned. |
| Meta Llama 3.1 8B foundation (preferred) | Validate conventional dense global-attention KV/state, static placement, and controller behavior without hybrid/multimodal confounders. | P7-01 owns selection; preferred pending Meta license acceptance, access, exact revision, converter support, and reproducible source/GGUF hashes. |
| Gemma 2 comparison (optional) | Additional dense-model comparison only. Gemma 2's interleaved sliding-window/global-attention pattern must be represented exactly; it is not described as a globally full-attention model. | Not the preferred foundation and not a phase gate. |
| Ornith-1.0-9B | Secondary capability and hybrid Qwen3.5-family stress cell. | Requires modality, main/mmproj, MTP, full-attention layer set, DeltaNet recurrent/convolution state, converter and operator-coverage contract. |
| Qwen3.5-9B | Lineage comparator, not an independent architecture control. | Candidate pending exact support/artifact pin. |
| 30B q4 | First static heterogeneous placement and host-capacity validation. | Capacity admitted in Phase 7; static run is first Phase 8 item. |
| 70B/90B | Early lower bounds, then only admitted measured-offload or rejection. | Execution issues stay gated until exact artifact admission. |

Ornith is not the generic foundation because its dense parameter routing uses a
hybrid Gated DeltaNet/full-attention text architecture and includes multimodal
source components. Certification requires license, immutable revision,
tokenizer/chat/tool/reasoning parsers, modality scope, main/mmproj/MTP contract,
architecture-state ledger, pinned llama.cpp/GGUF converter/operator coverage,
conversion/quantization recipe, hashes, and same-model quality baselines.

For a mixed quantization recipe such as `Q4_K_M`, the recipe label is not a
single tensor encoding. Evidence records the recipe and the actual per-tensor
`ggml_type`, block layout, shape, and byte count. Model-relevant reference or
candidate code must implement each encountered tensor type exactly.

## Language and Toolchain Decision

| Layer | Decision | Reason |
|---|---|---|
| Runtime, adapter, planner, governor | C++20 | Direct llama.cpp/GGML compatibility, established Windows toolchain, memory and ABI control. |
| NVIDIA kernels | CUDA C++ plus cuBLASLt/CUTLASS and GGML baselines | Native hardware access and profiler/tool support. |
| Offline analysis and quality | Python | Fast experimentation and existing evaluation ecosystem; not the runtime hot path. |
| Mojo | Isolated C ABI kernel-provider experiment only | Promising portable kernel language with C interop, but no native Windows support and application-level systems work remains on its roadmap. |
| Triton | Isolated kernel candidate | Useful autotuning reference; integration and Windows deployment must beat the baseline. |

The language decision can be reopened when a same-cell experiment proves a
better integration/performance/correctness result. It is not reopened by syntax
preference alone.

## Novelty Boundary

Current llama.cpp already exposes automatic fit behavior, GPU-layer and tensor
placement controls, mmap/direct-IO options, KV types, attention controls, and
speculative modes; its CUDA backend already performs internal dispatch.

PrismInfer may not claim these individual capabilities as novel. The
closest-work matrix in `novelty-and-gap-matrix.md` limits possible claims to:

- engineering integration over pinned GGML/GGUF on native Windows;
- evidence/cap/recovery/reproducibility methodology; or
- a new measured policy/mechanism result beyond the strongest composed
  baseline.

The following are research integration targets, not novelty by themselves:

- one exact, hardware-calibrated cross-resource plan;
- requested-versus-actual path evidence;
- explicit uncertainty and compatibility invalidation;
- actuator-bound lifecycle and recovery;
- a resource-DAG objective over critical-path time and observed bytes per
  committed output token;
- exact cap, quality, and claim governance;
- progressive representations and structured conditional policies only when
  integrated under the same evidence system.

Listing known variables jointly is not a novelty claim. Whether the integration
produces a non-obvious mechanism or material systems result is a research
question.

## Entry and Exit Gates

### Phase 7 entry

- [#109](https://github.com/Gravelaw/prisminfer/issues/109) supplies the pure
  workload-relative host-admission primitive and authoritative Windows commit
  source consumed by #82/#103/#84; it grants no hardware clearance by itself.
- [#103](https://github.com/Gravelaw/prisminfer/issues/103) is closed with
  retained supervisor, pre-context admission, watchdog, cancellation, and
  fault-injection evidence before any model-backed Phase 6/7 execution.
- Pinned llama.cpp/GGML commit and build flags.
- Native secure process boundary and artifact trust roots.
- Windows child/WDDM/host/file/pagefile evidence protocol.
- Actuator inventory and closest-work/current-versus-pinned audit.
- P7-01-selected foundation and Ornith hybrid stress artifact contracts with
  exact source, recipe, per-tensor `ggml_type`, and quant hashes.
- Exact selected-foundation/9B/30B/70B/90B capacity/resource-DAG lower-bound
  report.
- Model-relevant GGUF recipe/per-tensor-type inventory; exact custom reference
  semantics only for a claimed custom path.
- Same-cell CPU and llama.cpp/GGML CUDA baselines.
- Memory categories and quality fixtures sufficient for `research-only`
  calibration.

### Phase 7 exit

- In-process adapter or a documented blocker with an evidence-equivalent
  interface.
- Hardware/runtime fingerprint and invalidation tests.
- Real operator/shape/path trace.
- Raw calibration store and held-out prediction results.
- Static executable plan sidecar with actuator acknowledgement and R0/R1/R2
  recovery contract.
- Selected-foundation optimizer/oracle replay, followed by Ornith hybrid
  stress replay only when certified, under the selected cap tier.
- No configured budget serialized as an observed peak.
- Orchestration overhead and selection-accuracy gates pass.
- Exit audit classifies the exact cell without a generalized speedup claim.

### Phase 8 entry

- Phase 7 static plan replay/recovery is reliable.
- Conventional full-attention and admitted Ornith stress baselines are retained.
- 30B exact artifact passed early capacity admission.
- Activation/KV capture has fixture, privacy, and storage policy.
- Each research mechanism has an isolated baseline and quality gate.

### Phase 8 exit

- 30B static placement is measured or rejected.
- Each optional mechanism is independently passed/rejected; progressive and
  router branches are not mandatory.
- Approximate paths use pre-commit verification/rollback or explicit lossy
  classification.
- Joint optimization runs only if at least two mechanisms pass and cannot hide
  a failing component.

### Phase 9 exit

- Optional 30B dynamic work has a measured result or rejection.
- Only Phase 7-admitted 70B/90B artifacts have measured results; non-admitted
  artifacts remain early rejected research cells.
- No result relies on hidden pagefile/unified-memory capacity.
- Cross-hardware invalidation works.
- The roadmap states clearly which target is interactive, slow/offline,
  measured-offload, or infeasible.

## Reopen Conditions

Reopen the architecture council if any of these becomes true:

- llama.cpp/GGML cannot expose sufficient observations or controls without a
  broad fork;
- the in-process adapter makes cap evidence less trustworthy than the current
  process boundary;
- model architecture support makes the proposed foundation or 9B stress cell
  invalid;
- calibrated plans cannot outperform or reliably select ordinary llama.cpp
  configuration;
- conditional compute requires a model-training program larger than the runtime
  research scope;
- progressive representations cannot provide random access and bounded
  reconstruction;
- the 32 GiB host makes the 30B entry cell infeasible; or
- a new primary hardware target materially changes the CPU/GPU/memory hierarchy.
