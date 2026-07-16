# Adaptive Runtime Council Record

Council run: 2026-07-10 to 2026-07-11.

Status: council closeout complete. The research thesis receives a conditional
pass after adversarial dissent; the P0 deltas below supersede the provisional
architecture and are binding on Phase 7+ work.

## Mandate

The user asked the council to:

- design the proposed framework's functionality and architecture;
- define interaction with current runtimes and CPU/GPU/memory/storage;
- include GPU, CPU, compression, mathematical, and adversarial perspectives;
- deliberate in-scope and out-of-scope work;
- produce detailed workflow/process/diagram documents;
- produce a reference index;
- create a detailed roadmap in GitHub Project #2;
- produce an execution and testing plan; and
- ignore development cost when prioritizing.

The user subsequently required the council to include the complete session
reasoning and every relevant paper discussed. The portable council input is a
structured reconstruction in `session-thesis-and-evidence-map.md`, not a
verbatim transcript.

## Council Composition

| Role | Independent responsibility | Canonical trace |
|---|---|---|
| Council chair and systems architect | Reconcile current code, prior phases, session thesis, independent reviews, canonical architecture, and GitHub tracking. | Canonical documents in this directory. |
| GPU architect | CUDA/GGML integration, kernel/shape autotuning, streams/graphs, VRAM, actual-path telemetry, and GPU gates. | [`architecture.md`](architecture.md) and [`actuator-and-recovery-matrix.md`](actuator-and-recovery-matrix.md) |
| CPU and memory architect | CPU topology/kernels, RAM/commit/pagefile, mmap/NVMe, PCIe staging, process/in-process runtime boundaries, and large-model feasibility. | [`windows-evidence-protocol.md`](windows-evidence-protocol.md) and [`scale-capacity-and-bandwidth-admission.md`](scale-capacity-and-bandwidth-admission.md) |
| Compression specialist and applied mathematician | Formal optimization, rate-distortion/effective bits, progressive representations, KV, conditional compute, sample complexity, and falsification program. | [`optimizer-and-calibration.md`](optimizer-and-calibration.md) and [`threshold-registry.md`](threshold-registry.md) |
| Adversarial reviewer | Attempt to disprove feasibility, novelty, executability, evidence sufficiency, and roadmap sequencing after synthesis. | [Adversarial Review](#adversarial-review) and [`novelty-and-gap-matrix.md`](novelty-and-gap-matrix.md) |

The compression and mathematician roles were combined in one independent track
because representation theory, optimizer constraints, and quality risk are
coupled. Their formal findings remain distinct sections in the working paper.

## Deliberation Protocol

1. Each specialist researched primary/official sources and audited the current
   repository independently.
2. The user added the full session thesis and paper list to every specialist's
   brief.
3. Specialists produced separate working papers without editing one another's
   conclusions.
4. The chair compared agreements, disagreements, and executable repository
   boundaries.
5. Canonical scope, architecture, optimizer, testing, references, and roadmap
   documents were drafted.
6. A separate adversarial reviewer received the entire result and current code.
7. The reviewer issued five P0, seven P1, and two P2 findings.
8. The chair accepted the P0 dissent, recut the thesis and roadmap, and linked
   each documentation delta to a canonical contract below.

## Pre-Adversarial Specialist Convergence

The GPU, CPU/memory, and compression/mathematics tracks agreed on these facts
before adversarial review:

- PrismInfer currently has strong governance and evidence scaffolding but not
  the proposed adaptive runtime.
- The llama adapter is an external process/log parser; it cannot apply per-op
  plans, own streams, observe a graph, or certify child host peaks.
- Offload transfer fields are policy/config inputs, not real tensor movement.
- The pinned/staging `peak` values in the current offload plan are copied from
  configured budgets.
- The guarded q4 CUDA path is synthetic and not exact GGUF Q4_K_M semantics or
  performance evidence.
- The simulated 90B plan assumes 128 GiB host RAM; the current target has about
  32 GiB.
- Current upstream llama.cpp already overlaps with several proposed controls:
  fit-to-device behavior, tensor overrides, GPU layers, KV types, mmap/direct
  IO, and speculative modes. Exact availability must be checked against the
  pinned commit.

These findings make truthful runtime integration and measurement the next
dependency. They rule out beginning with a new matmul kernel or a learned
optimizer. The adversarial reviewer accepted these repository facts but
dissented from the proposed model cell, plan semantics, memory-certification
path, speculative objective, and roadmap coupling.

## Role Findings

### GPU architect

Primary conclusion:

> Embed the pinned runtime, trace real operations and actual paths, calibrate
> public controls, and add one narrow upstreamable dispatch/telemetry hook only
> when the 9B trace proves a useful choice.

Key reasoning:

- A CLI can select whole-run flags but not per-op MMVQ/MMQ/vendor/custom paths.
- A kernel cache key must include representation, exact shape/layout,
  phase, device/runtime pin, workspace, and power/thermal cell.
- GPU name and headline library benchmark are unsafe selection keys.
- Kernel wins must survive dequant/repack/workspace/transfer and full-model
  replay.
- One compute and one prefetch stream is the initial design; extra streams and
  CUDA graphs need critical-path evidence.
- 30B+ is first a placement/transfer problem, not a kernel problem.

### CPU and memory architect

Primary conclusion:

> Reuse GGML CPU kernels and its thread pool, calibrate the hybrid CPU topology,
> make child/in-process telemetry truthful, and treat RAM, commit, pinned
> staging, mmap residency, pagefile, and NVMe as distinct resources.

Key reasoning:

- The Intel Core Ultra 9 285H has heterogeneous cores; thread count alone is not
  a placement description.
- Pinned memory is a bounded staging resource, not a capacity tier.
- mmap makes an artifact addressable, not resident.
- A contiguous CPU/GPU split can be better than re-streaming weights every
  token because it transfers activation boundaries instead.
- On 32 GiB host RAM, dense-parameter 70B/90B q4 is normally
  storage-dependent before KV, workspace, and OS reserve.
- An artifact-specific bytes/bandwidth lower bound should reject unusable
  candidates before long runs.

### Compression specialist and mathematician

Primary conclusion:

> Use an offline evidence-constrained plan builder plus deterministic online
> replay, with lexicographic hard constraints and committed
> target-distributed-output objectives.
> Prioritize KV and boundary-activation compression; treat progressive weights
> and structured compute as derived-artifact/model-runtime research.

Key reasoning:

- The decision space is mixed discrete, stateful, and combinatorial; exact
  global online optimization is inappropriate.
- A single weighted score can hide a cap or quality violation.
- Coarse telemetry cannot identify kernel, transfer, queue, compression,
  thermal, and speculation costs; separate observations are required.
- Per-query recompression of immutable q4 weights is unlikely to amortize.
- Progressive precision is credible only with an explicitly nested
  base/residual artifact; separate Q2/Q3/Q4 files are not refinement streams.
- Prompt clustering is a prior for plan/prefetch, not a permanent semantic
  switch. Actual structured decisions require hidden state plus pre-commit
  verification/rollback or an explicitly lossy classification.
- Whole-layer skipping, learned KV merging, and aggressive dynamic precision
  often require model adaptation.

## Questions, Disagreements, and Resolutions

### Is the optimizer a learned model?

Initial session framing used "optimizer model." The council rejected a learned
model as an MVP requirement.

Resolution:

- start with safe enumeration, measurement, simple cost models, constrained
  search, and Pareto plans;
- online path is table/finite-state dispatch;
- learned ranking/bandits enter only after simple models fail a predeclared
  gate and can never bypass hard constraints.

### Does PrismInfer use its own runtime after calibration?

There were two possible meanings: replace llama.cpp, or own the controller.

Resolution:

- PrismInfer owns calibration, plan, policy, admission, fallback, and eventually
  bounded staging/dispatch;
- llama.cpp/GGML remains the graph/operator substrate;
- the next required step is an in-process adapter;
- a broad custom backend requires a council reopen.

### Does new matrix multiplication change the thesis?

AlphaTensor proves algorithm diversity and hardware-aware discovery. The GPU
and mathematical tracks agreed it does not replace the broader systems thesis.

Resolution:

- build a shape/representation-aware kernel registry;
- calibrate GGML/vendor/template/custom candidates fairly;
- retain algorithm discovery as a later offline lane;
- no new algorithm claim without exact q4 end-to-end evidence.

### Can compression happen per request?

The session proposed compressing/decompressing layers based on query/task.

Resolution:

- immutable weight representations are created offline;
- runtime selects a representation/tier and performs bounded fused/tiled
  reconstruction;
- KV and boundary activations are credible online compression objects;
- hot-path lossless q4 compression needs an entropy/random-access gate;
- source GGUF remains immutable.

### Can prompt clusters activate only needed neurons/layers?

Resolution:

- prompt/task cluster may choose an initial plan or prefetch prior;
- it may not permanently disable dense compute;
- run a hardware-aligned block oracle before training a router;
- hidden-state router plus confidence, OOD, dense sentinel/audit, and fallback
  is the minimum research design;
- whole-layer skipping remains model-adaptation scope.

### Is Ornith-1.0-9B the model?

Resolution:

- the Phase 6/Phase 7 foundation is one pinned, llama.cpp-supported
  conventional full-attention 8B/9B-class text model;
- Ornith is the second capability and hybrid multimodal stress cell, not the
  canonical controller foundation;
- Qwen3.5-9B is Ornith's lineage comparator, not an independent conventional
  architecture control;
- Ornith/Qwen3.5 contain 24 linear-attention layers and eight full-attention
  layers, so a uniform 32-layer KV formula is invalid;
- main GGUF, optional `mmproj`, MTP inclusion, converter revision/options,
  linear/recurrent/convolution state, and parser/template identity define
  separate validation-cell fields;
- pin and self-produce each quant only after conversion, operator coverage, and
  load smoke pass; and
- high model-card benchmark scores are not systems or local quality evidence.

### What does speculative execution optimize?

The provisional council optimized accepted draft tokens per time or transferred
byte. The adversarial reviewer showed that this assigns zero reward when no
draft token is accepted even though exact speculative decoding still emits one
target-distributed correction/extra token.

Resolution:

- `accepted draft tokens` are draft proposals retained by target verification;
- `committed target-distributed output tokens` are the accepted prefix plus the
  mandatory correction/extra target token that becomes externally visible;
- `rolled-back tokens` are discarded work and never enter the reward;
- `verified tokens` describe tokens whose distribution is established by the
  target verification procedure;
- primary objectives are expected committed target-distributed output tokens per
  cycle and per observed external byte;
- acceptance rate and accepted-draft length remain diagnostics; and
- approximate paths must verify before external commit or explicitly declare
  lossy semantics and restart/rollback behavior.

### C++20/CUDA or Mojo?

Resolution:

- C++20/CUDA is the production path because it matches llama.cpp/GGML, the
  current Windows toolchain, and NVIDIA profiling/ABI requirements;
- Python owns offline evaluation, not the hot path;
- Mojo does not natively support Windows and its application-level systems
  programming roadmap is not started, so it cannot replace the native runtime;
- Mojo does expose C FFI and a C ABI export facility, so Mojo/Triton/CUDA
  prototypes may implement an isolated versioned C ABI `KernelProvider` with
  explicit descriptors, stream, workspace query, error code, and no hidden
  allocation;
- every experiment provider must win same-cell correctness, memory, profiler,
  integration, and end-to-end evidence; and
- no language decision is treated as permanent dogma.

### How should 70B/90B be framed?

Resolution:

- program entry includes artifact-specific capacity and bandwidth lower bounds;
- the current host may make dense-parameter 70B/90B artifacts
  storage-dependent and unusable;
- speculation/conditional/native sparse structure must reduce or amortize bytes
  and pass separate quality gates;
- simulated, measured-offload, slow/offline, rejected, and validated remain
  distinct outcomes.

## Revised Binding Architecture

The chair accepts the adversarial dissent. This sequence supersedes the
provisional “unanimous” architecture:

```text
secure and observe the current process baseline
  -> certify one conventional full-attention foundation artifact
  -> certify Ornith as a separate hybrid multimodal stress artifact
  -> inventory only implemented pinned-runtime actuators
  -> calculate exact scale capacity and bandwidth lower bounds
  -> embed and trace llama.cpp/GGML
  -> replay one static acknowledged 8B/9B-class plan
  -> validate 30B static heterogeneous placement
  -> run independent optional kernel/KV/speculation/progressive/router hypotheses
  -> combine only independently passing mechanisms
  -> admit or reject exact 70B/90B artifacts
```

Every executable plan field binds to the normative
[actuator/acknowledgement/recovery matrix](actuator-and-recovery-matrix.md).
Recovery is local substitution before commit, compatible-boundary fallback, or
restart/reject. State-incompatible work has no continuity claim.

## Revised Scope

### Evidence-ready foundation

- secure native child launch and Job/process-tree accounting;
- conventional full-attention model certification;
- Ornith conversion, operator, main/`mmproj`/MTP, and state certification;
- Windows host/file/pagefile and WDDM/DXGI evidence protocol;
- exact actuator inventory and current-versus-pinned upstream audit; and
- early exact 9B/30B/70B/90B capacity/bandwidth lower bounds.

### Approved for Phase 7

- in-process adapter;
- exact fingerprints and invalidation;
- real trace and calibration data;
- public upstream control calibration;
- simple measured cost table and hard feasibility filter;
- one immutable static load/request-time plan containing only implemented
  actuators;
- requested-versus-actual acknowledgement; and
- exact conventional-foundation replay/audit, followed by separate Ornith
  stress replay.

Custom kernels and bounded staging are not Phase 7 dependencies.

### Early scale truth

- exact-artifact lower bounds run at program entry;
- 30B static contiguous CPU/GPU placement runs before optional adaptive
  mechanisms; and
- 70B/90B execution work is activated only after admission.

### Independent optional research

- living KV;
- static progressive derived artifact, followed only on a pass by runtime
  precision research;
- activation transfer compression;
- structured opportunity oracle and model-adaptation decision before any
  runtime router;
- committed-target-token-aware speculative offload;
- optional kernel hook/autotuner; and
- joint planning only after at least two independent mechanisms pass.

### Rejected or deferred

- clean-sheet runtime;
- per-query weight recompression;
- prompt-only permanent neuron/layer disabling;
- individual-neuron transfer;
- pagefile/unified memory as certified capacity;
- naive NVMe presented as interactive;
- arbitrary global online optimizer;
- new foundation-model training inside the runtime program;
- broad custom backend or multi-GPU serving before a reopen decision;
- per-epoch representation/placement/KV variables in the Phase 7 plan;
- generic mid-request fallback after incompatible state commits; and
- 70B/90B implementation backlog before exact-artifact admission.

## Post-Review Evidence Confidence

| Topic | Council confidence | Reason |
|---|---|---|
| Secure process/Windows evidence before plan replay | High as a dependency | Current shell launch, parent-only accounting, and WDDM ambiguity cannot certify the proposed controller. |
| Conventional full-attention foundation before Ornith | High | Ornith/Qwen3.5 hybrid linear/full attention and multimodal/MTP state confound the generic controller proof. |
| In-process adapter before controller execution | High | Current code cannot apply or acknowledge a plan. |
| Static actuator-bound planner plus deterministic replay | High | Search complexity, lifecycle limits, and safe recovery require a finite implemented action set. |
| Kernel portfolio/autotuning | High for feasibility, unknown benefit | Vendor/GGML mechanisms exist; exact-cell end-to-end result unknown. |
| Online KV compression | Medium-high | Strong paper precedent; exact artifact/context quality and overhead unresolved. |
| Progressive base/residual weights | Medium-low | Paper-backed idea, but static format, metadata, kernels, and quality must pass independently before runtime precision work. |
| Hidden-state structured compute | Low until oracle/adaptation evidence | A later audit cannot undo committed output and modern SwiGLU/hybrid-model opportunity is unknown. |
| Speculative offload | Medium | Exact target-distributed semantics are known, but committed-token reward, observed bytes, upstream baselines, and placement remain unresolved. |
| Interactive dense-parameter 70B/90B on current host | Low | 32 GiB RAM and transfer/storage lower bounds are severe. |

## Binding Post-Review Decisions

1. Current Phase 6 claim boundaries stay in force.
2. One conventional full-attention 8B/9B-class text model is the foundation
   cell. Ornith is a secondary hybrid multimodal stress cell; Qwen3.5 is its
   lineage comparator.
3. Phase 7 plans are static and actuator-bound. Candidate construction rejects
   any field without an implemented actuator and acknowledgement.
4. P7 in-process adapter and actual-path acknowledgement are the controller
   critical path, but secure native process launch and Windows evidence precede
   them.
5. Recovery is local substitution, compatible-boundary fallback, or
   restart/reject. No generic safe reversal is claimed after state/output
   commit.
6. No configured transfer/budget field is promoted as observed evidence.
7. Windows memory claims distinguish owned-allocation cap evidence from
   physical-residency/no-oversubscription evidence; NVML is corroborating under
   WDDM.
8. The optimizer is constraint-first and offline; runtime dispatch is finite
   and deterministic. A resource-constrained DAG makespan replaces additive
   overlap arithmetic for promoted schedules.
9. Speculative reward is committed target-distributed output tokens per cycle
   and per observed external byte. Accepted-draft metrics are diagnostic.
10. Existing pinned llama.cpp behavior is the baseline. Current `master` is a
    novelty check only and does not prove an actuator exists in the pin.
11. Source GGUF is immutable; derived artifacts have independent identity.
12. Prompt clusters only warm-start non-semantic prefetch. Runtime routing is
    optional research after oracle, adaptation, pre-commit semantics, and
    compatible-kernel gates.
13. Exact 30B/70B/90B lower bounds run early; 30B static placement precedes
    optional adaptive work; 70B/90B execution remains admission-gated.
14. C++20/CUDA is production. Mojo/Triton are isolated experiment providers;
    Mojo's C FFI/C ABI capability does not make it a mature native-Windows
    runtime.
15. Novelty is limited to an integration/evidence hypothesis until the gap
    matrix and a strongest-composed-baseline result pass.
16. Repository-root `Plan.md` is the program source of truth; GitHub Project #2
    is its operational mirror for the recut foundation, static-controller,
    early-scale, and optional-research epics.

## Adversarial Review

The independent reviewer issued a conditional pass for the research thesis and
rejected the original broad Phase 7-9 implementation plan as written. The chair
accepts that dissent. In particular, the word “unanimous” no longer applies to
the pre-review model choice, dynamic plan schema, fallback language,
speculative objective, Windows certification path, or roadmap order.

### P0 dissent

| Finding | Adversarial conclusion | Binding council response |
|---|---|---|
| AR-01, unexecutable decisions and unsafe transitions | The first optimizer included fields with no pinned-runtime actuator and promised reversal after incompatible state changes. | Phase 7 is static and actuator-bound; use the [actuator and recovery matrix](actuator-and-recovery-matrix.md), reject unsupported fields, and classify recovery precisely. |
| AR-02, Ornith invalidates the generic first-cell assumptions | Ornith/Qwen3.5 is hybrid linear/full attention with multimodal and MTP state, not a conventional full-attention controller cell. | Use a conventional full-attention 8B/9B-class foundation, Ornith as the secondary hybrid stress cell, Qwen3.5 as lineage comparator, and architecture-state accounting. |
| AR-03, schedule and speculative reward are wrong | Additive time does not compile an overlapped schedule; accepted draft tokens omit the mandatory target-distributed token. | Use resource-constrained DAG makespan and committed target-distributed tokens per cycle/observed external byte in [optimizer and calibration](optimizer-and-calibration.md). |
| AR-04, Windows/offload certification is unattainable from current evidence | Parent-only telemetry, configured transfer fields, NVML/WDDM ambiguity, and late scale bounds cannot certify the hierarchy. | Apply the [Windows evidence protocol](windows-evidence-protocol.md), separate owned allocation from residency claims, and run [scale admission](scale-capacity-and-bandwidth-admission.md) early. |
| AR-05, external adapter is unsafe | Shell command construction and path/temp handling cannot be the plan-driven trust boundary or provide child-tree evidence. | Native process/Job launch and artifact/provider trust controls are foundation gates in the [security and reproducibility contract](security-privacy-reproducibility.md) and roadmap. |

### P1 and P2 response

- Thresholds and sampling plans are centralized in
  [`threshold-registry.md`](threshold-registry.md); provisional values cannot
  promote claims without their metric-specific authority.
- Designed experiments, nested holdouts, and fresh confirmatory replay replace
  causal claims from coarse observational telemetry.
- Progressive representation and runtime routing are independent optional
  hypotheses, not Phase 8 or 30B prerequisites.
- A router audit after output commit does not preserve semantics. The path needs
  pre-commit verification/rollback or an explicit lossy classification.
- [`novelty-and-gap-matrix.md`](novelty-and-gap-matrix.md) limits current
  novelty to an integration/evidence hypothesis.
- The roadmap is recut into evidence foundation, static controller, early scale
  truth, and independent research branches.
- Threat, privacy/retention, artifact authenticity, provider trust, and
  reproducibility are explicit in
  [`security-privacy-reproducibility.md`](security-privacy-reproducibility.md).
- C++20/CUDA remains binding for production. Mojo has real C FFI/C ABI
  facilities but lacks native Windows support and application-runtime maturity;
  it is limited to an isolated provider experiment.
- Canonical terminology now separates conventional full-attention, dense
  parameter routing, hybrid linear/full attention, accepted drafts, committed
  target-distributed output, verification, and rollback.

### P0 documentation-delta disposition

“Applied” below means the architecture/roadmap contract has been revised. It
does not mean the corresponding code or evidence already exists.

| Delta | Canonical disposition |
|---|---|
| Actuator, acknowledgement, and recovery matrix | [`actuator-and-recovery-matrix.md`](actuator-and-recovery-matrix.md) |
| Conventional foundation plus Ornith stress cell | [`scope-and-requirements.md`](scope-and-requirements.md), [`9b-optimizer-experiment.md`](9b-optimizer-experiment.md) |
| Committed-token reward and DAG makespan | [`optimizer-and-calibration.md`](optimizer-and-calibration.md) |
| Windows child/Job, WDDM/DXGI, host/file/pagefile protocol | [`windows-evidence-protocol.md`](windows-evidence-protocol.md) |
| Early 9B/30B/70B/90B admission | [`scale-capacity-and-bandwidth-admission.md`](scale-capacity-and-bandwidth-admission.md) |
| Native launcher and artifact/provider trust | [`security-privacy-reproducibility.md`](security-privacy-reproducibility.md), [`roadmap.md`](roadmap.md) |
| Metric thresholds, sampling, and confirmatory replay | [`threshold-registry.md`](threshold-registry.md), [`execution-and-testing-plan.md`](execution-and-testing-plan.md) |
| Optional progressive/router branches and recut sequence | [`roadmap.md`](roadmap.md) |
| Upstream/closest-work gap and allowed novelty | [`novelty-and-gap-matrix.md`](novelty-and-gap-matrix.md) |
| Portable session record and primary sources | [`session-thesis-and-evidence-map.md`](session-thesis-and-evidence-map.md), [`references.md`](references.md) |

## Final Verdict

**Conditional pass, with adversarial dissent incorporated.**

The foundation and static-controller program may proceed. The original dynamic
Phase 7 plan, mandatory Phase 8 bundle, and unconditional 70B/90B execution
backlog do not. A mechanism becomes implementation work only through its
actuator, evidence, security, statistical, and exact-cell gates.

No 9B, compression, custom-kernel, speculative-offload, or large-model
performance claim is created by this council record. If the static controller
cannot predict or improve on the strongest upstream sweep, the valid outcome is
a calibration/evidence tool and rejection of the broader runtime hypothesis,
not automatic expansion into more adaptive mechanisms.

## Final Design-Freeze Addendum (2026-07-11)

The council reconvened after the environment/safety audit and code inspection.
This addendum supersedes earlier model-choice, quantization and sequencing
language where they differ.

Frozen thesis:

> PrismInfer is a safety-supervised, calibration-driven control plane and plan
> executor over pinned llama.cpp/GGML/GGUF. For one exact
> hardware/runtime/model/service cell, it performs two-stage admission,
> measures execution, enumerates only implemented and acknowledged actuators,
> builds one conservative static plan offline, and deterministically replays it
> in a contained worker.

Binding corrections:

1. Issue #103 owns the fail-closed outer supervisor, exclusive GPU lease,
   two-stage admission, watchdog, cancellation, abort evidence and fault suite.
   Safety prerequisites #81/#82 advance before model-backed Phase 6 evidence;
   phase numbering never overrides this dependency.
2. The preferred foundation is a self-produced, hash-pinned Llama 3.1 8B
   Instruct text GGUF, subject to accepted license/access. Ornith-1.0-9B is the
   distinct hybrid stress cell. Gemma 2 is not a globally full-attention
   control because it alternates local and global attention.
3. `Q4_K_M` denotes a mixed quantization recipe/file type. Exact truth and
   fixtures are per tensor and per actual `ggml_type`; eligible custom kernels
   may cover only an acknowledged subset.
4. Mandatory Phase 6 outcomes are safety clearance, strict retained evidence,
   exact quant truth, quality fixtures and a supervised same-cell result or
   rejection. Offline KV and custom-kernel speedup are optional hypotheses; the
   `>=1.10x` threshold governs only a promoted custom-kernel speedup claim.
5. Controller success and optimization benefit are separate. Safely choosing
   the strongest upstream plan can validate the controller, but does not prove
   a speedup. If selector regret/overhead or admission truth fails, PrismInfer
   remains an admission/evidence tool.

The canonical clearance matrix, dependencies, statuses and exact issue map are
in repository-root [`Plan.md`](../../Plan.md).
