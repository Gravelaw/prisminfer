# Major Milestones

This document maps desired outcomes to the authoritative packet and issue order
in [`Plan.md`](../../Plan.md). It does not copy live issue status. If this
document conflicts with the Plan, the Plan wins.

Each milestone records a valid negative outcome. A rejection, unsupported seam,
slower provider, or upstream-baseline win is a completed evidence result when
the declared artifacts and review gates are satisfied.

## Order and clearance rule

~~~mermaid
flowchart LR
  M0["M0: V2 freeze"] --> M1["M1: Packet A"]
  M1 --> M2["M2: Packet B"]
  M2 --> M3["M3: Packet C"]
  M3 --> M4["M4: Packet D"]
  M4 --> M5["M5: Packet E"]
  M5 --> M6["M6: Packet F"]
  M6 --> M7["M7: Packet G"]
~~~

The Plan packet table is the conservative single-goal order. Packet A precedes
Packet B but remains CPU/offline only. Packet B must close before model-backed
execution. Packet C requires both exits. This reconciles the Plan's safety-first
critical path with its packet contract without reordering issues.

No milestone text authorizes downloads, CUDA, model execution, calibration,
benchmarking, sustained hardware work, or self-hosted workflows. Those require
the exact Plan clearance and, for T3 work, separate explicit user authorization.

## M0: Adaptive Runtime V2 governance freeze

**Outcome.** Replace the duplicated V1 research packet with nine canonical,
single-owner documents, corrected mathematics, a current novelty boundary, and
one milestone coordinate system.

**Issue sequence.** Documentation-only migration; it does not create or reorder
an implementation issue. Any merge follows the repository's ordinary bounded
documentation/governance workflow.

**Predecessor and clearance.** Current root policy and Plan are read from the
exact main base. C0 only.

**Allowed work.**

- documentation and link changes;
- primary-source review;
- Markdown/math/Mermaid/schema validation;
- atomic archive migration after V2 validation.

**Prohibited work.**

- implementation changes;
- model/artifact downloads;
- CUDA, model, calibration, or benchmark execution;
- GitHub/Project status changes not separately authorized;
- implying that V2 itself grants a later clearance.

**Deliverables.**

- the nine V2 documents;
- sole-owner/authority map;
- council, devil's-advocate, architect, and planner dispositions;
- active-link migration and immutable V1 archive;
- reconciled `Plan.md`, Project #2 README/fields, and active Packet A/B PR
  references;
- validation receipt.

**Retained artifacts and tests.**

- exact base SHA and changed-path inventory;
- UTF-8, internal-link, legacy-delimiter, dollar-block, fence, and Mermaid
  checks;
- no active references to the old directory;
- `git diff --check`, Plan/Project synchronization, and Project readback.

**Claim boundary and negative outcome.** Documentation readiness only. A failed
link, rendering, authority, or archive check blocks migration/merge but changes
no runtime result.

**Review tier.** T0 for mechanics; T2 independent exact-head review because the
packet changes governing safety, evidence, and provider contracts.

## M1: Packet A - exact quant truth, runner, and immutable cells

**Outcome.** Exact per-tensor GGML quantization truth, a strict manifest-emitting
runner, and immutable tiny/Llama/Ornith/smoke artifact identities.

**Issue sequence.** #74, then #75, then #80.

**Predecessor and clearance.** Completed bootstrap. C0 CPU/source/fixture work.
Later issue-specific acquisition still requires its own authorization and
artifact rules.

**Allowed work.**

- canonical CPU reference semantics and differential fixtures for every
  per-tensor `ggml_type` in the exact artifact;
- strict schema, manifest, comparator, path, and hash tooling;
- exact model/source/tokenizer/template/recipe selection and pin records;
- a CPU/offline entropy, effective-rate, rotation-legality, random-access, and
  rate-distortion census after the source is lawfully available.

The early census is diagnostic only. It earns no derived-artifact, provider,
quality, capacity, or performance credit before #94.

**Prohibited work.**

- CUDA or model execution;
- calling `Q4_K_M` one uniform encoding;
- requantizing an existing lossy Q4 artifact as the research source;
- runtime provider/kernel integration;
- paper-to-local performance extrapolation.

**Deliverables.**

- exact per-tensor inventory and golden fixtures;
- canonical reference decoders and strict evidence runner;
- immutable artifact records for Llama 3.1 8B, Ornith 9B, and smoke cells;
- diagnostic representation-census schema and non-credit classification.

**Retained artifacts and tests.**

- source/recipe/output hashes and tensor tables;
- property, boundary, malformed-input, overflow, fuzz, and differential tests;
- manifest round-trip and fail-closed missing-field tests;
- artifact-license/access and approved-root receipts.

**Claim boundary and negative outcome.** CPU truth and artifact identity only.
Unsupported tensor semantics, inaccessible artifacts, a non-random-access
format, or an unpromising census is a valid stop result.

**Review tier.** T1 for ordinary CPU fixtures; T2 for untrusted parsers,
artifact trust, evidence governance, or allocation arithmetic.

**M1 closeout (2026-07-18).** Issues #74, #75, and #80 are complete in binding
order through Packet A PR #111. The retained closeout covers the complete
292-tensor Llama inventory and exact CPU differential slices, strict
fail-closed evidence bundles, and immutable positive/negative/deferred model
cell descriptors. The foundation artifact acquisition is the recorded one-time
owner-authorized exception; it does not convert third-party production into a
self-produced claim. All work and validation remained CPU/offline. M1 grants
no CUDA, model-execution, quality, calibration, capacity, performance, C2, or
sustained-hardware clearance.

## M2: Packet B - secure worker and hardware boundary

**Outcome.** A native contained worker, minimum independent Windows/WDDM/host
evidence, and a fail-closed supervisor with staged admission.

**Issue sequence.** #81, then #82, then #103.

**Predecessor and clearance.** M1 packet exit under the conservative packet
contract. C0 for implementation and simulation; only the exact Plan-authorized
C1 tiny attended fixture may touch CUDA before C2 closes.

**Allowed work.**

- explicit native process creation, suspended worker, Job assignment, controlled
  environment/handles, heartbeat, cancellation, and cleanup;
- authoritative host physical/commit and WDDM/DXGI telemetry;
- simulated and tiny-fixture fault injection;
- T-100 through T-105 implementation and evidence.

**Prohibited work.**

- 8B/9B/30B model execution;
- calibration or performance claims;
- treating NVML, mmap, pagefile, or configured budgets as residency proof;
- worker-owned policy or same-context automatic retry.

**Deliverables.**

- worker/supervisor boundary and signed executable identity;
- two-stage admission receipt;
- independent telemetry and watchdog;
- cancellation/recovery state machine and fault evidence;
- exact-head safety review.

**Retained artifacts and tests.**

- command/handle/Job/process-tree evidence;
- stale/missing/contradictory sensor, reserve breach, heartbeat loss, worker
  hang, child escape, timeout, cleanup, and lease tests;
- host/DXGI source and unit validation;
- tiny-dispatch bound when separately authorized.

**Claim boundary and negative outcome.** Hardware-safety prerequisite only.
Failure to observe or control the worker/device blocks C2 and all model work; it
does not justify bypassing the supervisor.

**Review tier.** T2 exact-head safety/security review; any tiny live hardware
evidence is T3 and separately authorized.

**M2 implementation receipt (2026-07-18, review state).** Packet B is ported
onto the post-M1 `main` base without restoring the archived V1 packet. The
retained CPU/source/simulation implementation binds approved executable
identity to suspended non-breakaway Job containment, explicit process/memory/
time limits, exact child-tree cleanup, fresh independent Windows evidence,
workload-relative #109 host admission, exclusive device lease, staged one-shot
tokens, watchdog submission blocking, cooperative-cancel/Job-abort deadlines,
and receipt-bound cleanup quarantine. Owned-GPU and WDDM evidence carry bounded
capture freshness, and executable approval rejects unheld intermediate path
components. This receipt is not the packet exit: C2 stays closed
until final hosted checks and a fresh independent exact-head safety/security
review accept the complete tree. No CUDA, model, calibration, capacity, or
performance evidence was produced.

The first fresh review of head `2effb8497c2b44328faeb7c8f93e1fcc0ea4deed`
rejected packet exit. The CPU-safe follow-up now removes caller-assembled
containment, token-consumption, cancellation, and Job-cleanup transitions from
the production session API. `GpuAdmissionSession` launches the approved image,
retains the process/Job/control authority through exit, derives protocol
deadlines from the immutable Stage-A receipt, and owns one-shot token delivery,
heartbeat rejection, cooperative cancellation, Job termination, and cleanup
facts. The raw runner requires a session-private capability; evidence callbacks
and live process-memory sampling are bounded independently, with an emergency
Job watchdog retained while they execute. Token delivery also has an explicit
consumption timeout. A dedicated nonce-bound control pipe is separate from worker output.
Duplicate consumption, missing context readiness, heartbeat loss, and cleanup
are covered by contained CPU worker tests. The production llama GPU path fails
closed until it implements this context-ready protocol. The Windows evidence
producer samples WDDM GPU Process Memory while the retained process is live,
binds PID plus DXGI LUID, and uses NVML only as a non-WDDM fallback. Missing,
stale, mismatched, duplicate, or contradictory reports fail closed. This remains review-state
implementation only: no tracker state or clearance changes occur until fresh
exact-head review and hosted checks accept the final tree.

## M3: Packet C - exact admission and supervised foundation evidence

**Outcome.** Exact capacity/bandwidth lower bounds, frozen quality fixtures, one
supervised same-cell foundation result, and the Phase 6 audit.

**Issue sequence.** #84, then #76, then #77, then #78.

**Predecessor and clearance.** M1 and M2 exits, including C2. The one-shot Llama
foundation warmup/baseline uses C3 and its exact admission token.

**Allowed work.**

- exact admission reports for 8B/9B/30B/70B/90B artifacts without executing
  rejected cells;
- paired quality/retrieval/task fixtures and frozen sample plans;
- strongest public llama.cpp control sweep and the declared PrismInfer
  foundation comparison for Llama 3.1 8B;
- 10 GiB and 12 GiB primary requested tiers, physical-device reference, and
  optional 8 GiB stress-only tier;
- complete security/evidence/claim audit.

**Prohibited work.**

- Ornith model execution at this milestone;
- calibration-driven selection;
- custom representation/provider, KV, speculation, routing, or joint claims;
- executing 30B/70B/90B merely because storage or arithmetic size looks small.

**Deliverables.**

- exact resource-DAG admission or rejection for every named artifact/cap;
- quality and output-length baseline;
- same-cell upstream and PrismInfer Llama 8B evidence;
- Phase 6 audit classification.

**Retained artifacts and tests.**

- AdmissionReceipts with every memory/state/workspace/transfer term;
- manifests, raw events, allocator/backend reconciliation, timelines, and
  paired quality results;
- cold/warm phase separation, randomized trial order, failure and missingness
  tests;
- T-001 through T-008 decisions.

**Claim boundary and negative outcome.** One exact foundation cell only. An
upstream win, usability-floor miss, quality failure, or pre-run rejection is a
valid result and cannot be generalized to a model family.

**Review tier.** T2 for the audit and admission logic; T3 for the separately
authorized model evidence.

## M4: Packet D - actual-path adapter, seam proof, and calibration substrate

**Outcome.** A pinned actuator inventory, an in-process actual-path adapter with
a proven or rejected GGML seam, and an immutable calibration/sample-plan store.

**Issue sequence.** #83, then #85, then #86.

**Predecessor and clearance.** M3/Phase 6 audit exit. M4 begins with source,
CPU, simulation, and already cleared tiny-fixture work. The #86 exit establishes
C4; fresh supervised foundation calibration begins in M5, not M4.

**Allowed work.**

- inventory public runtime controls and exact acknowledgement paths;
- replace shell/log scraping with worker-contained in-process lifecycle;
- execute the tiny seam-proof spike defined in
  [`architecture.md`](architecture.md);
- collect requested/actual operator, placement, allocation, workspace, state,
  and transfer traces for the admitted tiny seam;
- build normalized calibration entities, fingerprints, partitions, drift rules,
  and abstention tests.

**Prohibited work.**

- freezing a provider ABI without the seam proof;
- automatic adaptive replay;
- fresh 8B/9B calibration or model execution;
- 30B execution or optional-mechanism performance claims;
- silently maintaining a broad llama.cpp fork;
- using search/validation data as promotion confirmation.

**Deliverables.**

- actuator/acknowledgement/recovery inventory;
- in-process external-equivalence and actual-path evidence;
- seam-proof pass or structural-failure report;
- immutable calibration store and frozen sample plans.

**Retained artifacts and tests.**

- exact dependency/source/build hashes;
- public-control and fallback traces;
- hidden allocation/workspace, ignored/clamped setting, unsupported operator,
  state-lifecycle, and pin-change tests;
- held-out prediction and missing-feature/abstention fixtures.

**Claim boundary and negative outcome.** Substrate and calibration readiness,
not an optimization win. A failed seam triggers the architecture reopen review
and evaluation of narrower hooks or another mature substrate; it does not
directly authorize a clean-sheet runtime.

**Review tier.** T2 for native adapter, provider boundary, and governance; T3
for any separately authorized foundation calibration evidence.

## M5: Packet E - calibrated static controller proof

**Outcome.** A conservative feasible-set selector, measured feasible-oracle
comparison, immutable acknowledged replay, and foundation confirmation.

**Issue sequence.** #87, then #88, then #89.

**Predecessor and clearance.** M4 exit and C4. #88 opens C5 static replay; #89
closes C6 only after its fresh audit.

**Allowed work.**

- execute the frozen #86 Llama 8B calibration plan at separately admitted
  10 GiB and 12 GiB requested tiers under C4;
- resource-DAG candidate enumeration over supported public/static actuators;
- held-out prediction, Pareto/lexicographic selection, and abstention;
- immutable plan replay with actual acknowledgements;
- R0/R1/R2 fault/recovery evidence;
- fresh Llama 8B confirmation;
- Ornith 9B only as a separately admitted hybrid-state stress cell after the
  foundation result.

**Prohibited work.**

- treating representation identity as a static-plan actuator;
- optional compressed providers, speculative offload, or structured routing;
- exact 30B execution;
- combining Llama and Ornith results into one architecture-wide claim.

**Deliverables.**

- feasible-oracle and selector-regret report;
- certified static plan bundle and recovery graph;
- T-020 through T-025 results;
- Llama confirmation, separate Ornith stress result/rejection, and Phase 7
  audit.

**Retained artifacts and tests.**

- complete candidate frontier and rejection rationales;
- calibration/promotion partition hashes;
- actual acknowledgement, drift, stale-plan, cap-pressure, incompatible-state,
  and recovery tests;
- fresh randomized confirmation evidence.

**Claim boundary and negative outcome.** Safe static selection for exact cells.
Selecting upstream, abstaining, rejecting Ornith, or finding no controller
advantage is valid.

**Review tier.** T2 controller/recovery review and T3 separately authorized
model/calibration evidence.

## M6: Packet F - exact 30B truth and independent optional providers

**Outcome.** One exact 30B static placement result or rejection, followed by
independent pass/reject decisions for every optional mechanism and a Phase 8
audit.

**Issue sequence.** #90; then independently checkpointed #91 through #95; then
#96.

**Predecessor and clearance.** M5/C6 exit and exact #84 admission. Accepted #90
static truth establishes C7. Under C7, each #91 through #95 mechanism requires
its own issue-specific entry and evidence gate; #96 establishes C8 only after
every independent decision is retained.

**Allowed work.**

- freeze and measure the strongest exact 30B static CPU/GPU placement;
- #91 phase-specific kernel dispatch, bounded staging, and activation-transfer
  hypotheses;
- #92 separate KV and architecture-state policy;
- #93 committed-output-aware speculative offload;
- #94 representation compiler, legal rotations, fixed hot base, progressive
  prefixes, cold-residual entropy, and direct execution provider;
- #95 structured-compute oracle followed by a guarded router only if the oracle
  passes;
- record joint-optimization eligibility only after at least two mechanisms pass
  independently; Packet G owns any combined plan or execution.

**Prohibited work.**

- opening optional mechanisms before #90;
- hiding a failed component inside a joint result;
- sharing one pass between decode, prefill, and KV/state paths;
- joint candidate execution, combined planning, or joint-manifest generation;
- full persistent reconstruction, source-tensor duplication, or lossy-Q4
  requantization;
- calling distillation/model adaptation a runtime provider;
- claiming entropy/rotation/progressive novelty.

**Deliverables.**

- exact 30B static result or lower-bound rejection;
- independent pass/reject/not-admitted record for #91-#95;
- phase-specific provider descriptors and actual-path evidence where applicable;
- factorial identity/rotation/residual-routing ablation;
- T-040 through T-049 decisions and Phase 8 audit.

**Retained artifacts and tests.**

- strongest same-cell upstream sweeps and full-model replays;
- source-duplication, random-access, maximum-expansion, workspace, decode,
  prefill, KV/state, transfer, recovery, profiler, quality, retrieval, and
  output-length evidence;
- isolated microbenchmarks linked to end-to-end confirmation;
- independent candidate manifests and a joint-eligibility decision record; no
  joint candidate manifest is produced before Packet G admission.

**Claim boundary and negative outcome.** Exact mechanism/cell/phase only.
Capacity-only, slower, quality-failing, provider-unsupported, and not-admitted
results are valid. A microkernel win alone is not a provider pass.

**Review tier.** T2 for providers, parsers, CUDA, and safety contracts; T3 for
every separately authorized model/hardware evidence cell.

## M7: Packet G - gated scale and final classification

**Outcome.** Refreshed scale admission, only admitted dynamic/large-model
results, portability/invalidation evidence, and the final program audit.

**Issue sequence.** #97, then #98, conditional #99/#100, then #101 and #102.

**Predecessor and clearance.** M6/#96 establishes C8. Under C8, #97 refreshes
admission; #98 and conditional #99/#100 may run only for their separately
admitted exact cells; #101 tests portability/invalidation; and the #102 final
audit establishes C9. C9 is the final classification, not an entry clearance
for the work that establishes it.

**Allowed work.**

- refresh all exact 30B/70B/90B capacity, bandwidth, storage, host, state, and
  safety bounds;
- optional admitted 30B dynamic result;
- exact 70B and 90B execution only when each corresponding issue is admitted;
- portability, invalidation, pin-update, recalibration, security, evidence, and
  claim audits.

**Prohibited work.**

- executing a rejected scale cell;
- treating T-060 through T-065 as permission to bypass admission;
- generalizing one quant, context, cap, workload, or device to a size bucket;
- hiding storage/pagefile/unified-memory pressure;
- publishing a final speedup without strongest same-cell controls.

**Deliverables.**

- refreshed admission receipts;
- exact dynamic/scale results or rejections;
- portability and two-pin seam-maintenance report;
- final security/evidence/claim classification and runtime-direction decision.

**Retained artifacts and tests.**

- all exact manifests, raw observations, timelines, quality/output-length
  results, T-060 through T-065 decisions, and negative bounds;
- invalidation/recalibration, fallback, cleanup, pin-update, and reproducibility
  tests;
- final T2/T3 review receipts.

**Claim boundary and negative outcome.** Exact admitted cell only. A resource-DAG
rejection, usability miss, or upstream win is a successful final classification.

**Review tier.** T2 final governance/security review and T3 for every separately
authorized model/hardware result.

## Foundation and representation pilot

The pilot is staged by the milestones above; this section does not create a
parallel schedule.

### Cells and order

1. Tiny deterministic fixtures prove semantics, safety, and the integration
   seam.
2. Llama 3.1 8B is the first conventional model cell.
3. Ornith 9B follows only as a separately admitted hybrid-state stress cell.
4. Requested 10 GiB and 12 GiB tiers are primary; device-reference is separate;
   8 GiB is stress-only.

### Controls

For each exact cell retain:

- strongest public llama.cpp auto-fit/default plan;
- full supported public-control sweep;
- CPU-only feasible control;
- same-artifact static PrismInfer plan;
- source/representation and decompressed controls where a provider is admitted.

### Diagnostic arms

Packet A may measure, offline and without credit:

- source BF16/FP16 and exact GGUF payload/scales;
- identity versus legal random/learned rotations;
- scalar/lattice rates at matched effective bits;
- fixed chunks versus entropy-coded residual chunks;
- base plus each valid residual prefix;
- index, padding, codebook, checksum, expansion, and random-access cost.

Hot decode, prefill, KV/state, staging, speculation, and structured-compute arms
remain closed until their Packet F issue. The early census may reject them
before runtime implementation.

### Required measurements

- effective artifact and resident bytes, source duplication, workspaces, pools,
  fragmentation, staging, and architecture state;
- file, H2D, D2H, and inter-provider traffic;
- cold load, warm prefill, TTFT, batch-1 decode, p95 ITL, long-context decode,
  committed output, and energy;
- achieved bandwidth, timeline overlap, occupancy/register pressure, and actual
  kernel path where applicable;
- paired task quality, cross-entropy/KL diagnostics, retrieval failures,
  negative samples, and output-length distribution.

Search, validation, confirmation, and promotion are separated. Decode and
prefill pass independently. All comparisons use the threshold-owned sample
plan and strongest same-cell control.

## Requirements traceability

This matrix preserves the V1 requirement identifiers without creating another
owner for their detailed contract.

| Requirement group | Canonical V2 owner | First proving milestone | Terminal evidence |
|---|---|---|---|
| FR-001 through FR-004: fingerprint, identity, pinning, invalidation | Charter; evidence | M1, completed through M4/M7 | ValidationCell, ArtifactRecord, invalidation receipt |
| FR-010 through FR-014: calibration, raw samples, shapes, controls, holdout | Mathematics; evidence | M4 | CalibrationTrial set and prediction/selection report |
| FR-020 through FR-025: feasible planning, cap/quality invariants, DAG, Pareto, recovery rationale | Mathematics; safety | M4/M5 | Candidate frontier, rejection reasons, oracle/regret evidence |
| FR-030 through FR-035: immutable static plan, acknowledgement, bounded hot path, recovery | Safety | M5 | CertifiedPlanBundle and R0/R1/R2 receipts |
| FR-040 through FR-045: provenance, reconciliation, transfers, Job/host, actual path, profiler | Evidence; safety | M2, completed through M4/M6 | AdmissionReceipt, raw events, timelines, profiler hashes |
| FR-050 through FR-056: effective bytes, quality, derived artifacts, random access, routing/audit separation | Mathematics; research; evidence | M1 diagnostic, binding at M6 | Artifact/provider records and independent mechanism decisions |
| NFR-001: correctness | Evidence | M1 | Differential/reference fixtures |
| NFR-002 and NFR-002A: safety and supervision | Safety; evidence | M2 | Fault suite and C2 evidence |
| NFR-003: reproducibility | Evidence | M1, completed at every audit | Reproducibility bundle |
| NFR-004: explainability | Safety; evidence | M4/M5 | Plan rationale and actual acknowledgements |
| NFR-005: hot-path overhead | Evidence | M5/M6 | T-008 and provider overhead evidence |
| NFR-006: stability | Evidence | M3 onward | Repeatability/drift results |
| NFR-007: portability | Architecture; evidence | M7 | Pin/platform invalidation report |
| NFR-008: upstreamability | Architecture | M4 | Seam-proof and patch-scope decision |
| NFR-009: security/authenticity | Evidence; safety | M1/M2/M6 | Parser/process/provider security receipts |
| NFR-010: privacy | Evidence | M3 onward | Data-retention and fixture provenance |
| NFR-011: experimental providers | Architecture; evidence | M6 | Provider registry, ABI, cap, recovery, and full-model evidence |
