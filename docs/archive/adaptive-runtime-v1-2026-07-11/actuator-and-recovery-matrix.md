# Actuator, Acknowledgement, and Recovery Matrix

Status: normative constraint for every executable PrismInfer plan.

An optimizer variable is not executable merely because a paper or llama.cpp
configuration describes it. Every plan field must bind to an implemented,
pinned actuator and acknowledgement contract.

## Required Actuator Descriptor

```text
actuator_id and version
integration_tier
pinned API, CLI option, or backend hook
lifecycle = process | model_load | context_create | request | token | operator
eligibility predicate
requested value schema
actual acknowledgement schema
memory/residency effect
semantic/state effect
safe application point
recovery_class
fallback target
fault-injection evidence
```

Candidate construction rejects a plan field when any required descriptor field
is missing or the active pinned runtime cannot acknowledge it.

## Recovery Classes

### R0: local substitution before commit

The candidate has not changed externally committed token/model state. The
runtime can use an equivalent implementation for the same operator and inputs.

Examples:

- candidate kernel rejected before launch -> GGML default kernel;
- prefetch miss -> wait for the ordinary resident/source path;
- optional profiler/telemetry path unavailable -> execute without the claim.

Required proof:

- same graph/operator semantics;
- no partial output/KV/state commit;
- workspace cleanup;
- requested and actual variant evidence.

### R1: boundary fallback with compatible state

The plan changes at a declared request/token/graph boundary and the destination
plan can consume exactly compatible state, or a versioned state conversion is
performed before new output is committed.

Examples, only after implementation evidence:

- thread-count policy change at a backend-supported boundary;
- speculation disabled after a completed verification cycle;
- KV representation transition with an exact conversion and sufficient
  workspace;
- transfer/prefetch policy change while tensor representation and residency
  invariants remain valid.

Required proof:

- boundary definition and acknowledgement;
- state compatibility/conversion hash and correctness;
- transition memory/time;
- no user-visible output produced during a failed conversion;
- fallback conversion or request restart on failure.

### R2: restart or reject

The decision is load/context-time, changes model representation/graph/state, or
cannot be safely reversed. The current request is restarted from a valid
checkpoint or rejected. It is not described as seamless fallback.

Examples:

- source/derived artifact or quant representation;
- GPU layer count or tensor buffer override chosen at model load;
- mmap/direct-IO/mlock behavior;
- context size, KV layout/type without a proven conversion;
- flash-attention/context backend configuration;
- draft model or MTP artifact loading;
- multimodal projector inclusion/placement;
- structured graph/router provider incompatible with dense graph state.

Required proof:

- no output continuity claim;
- explicit restart/reload cost;
- checkpoint scope if restart is offered;
- clear user/run classification.

### R3: quality-bounded approximation

The candidate may produce a different hidden state, KV/recurrent state, token,
or output. A later audit cannot undo already committed output. The run is
explicitly lossy/approximate unless pre-commit verification exists.

Examples:

- structured block omission;
- lossy progressive precision change;
- KV eviction/merging without exact verification;
- approximate early exit.

Contract options:

1. **Pre-commit verification:** exact/dense check completes before token and
   state commit; the approximate result can be replaced.
2. **Checkpointed rollback:** state and output are quarantined, with a bounded
   rollback window and recomputation cost.
3. **Explicit approximation:** output may differ; audits only disable future
   use and do not claim semantic recovery.

## Actuator Matrix

`Proven` means proven in the selected pinned build and PrismInfer adapter, not
available in current upstream master alone.

| Plan decision | Likely pinned actuator | Lifecycle | Phase 7 status | Acknowledgement | Recovery |
|---|---|---|---|---|---|
| Model/source GGUF | libllama model load | model load | Required | opened file identity, hash, loader/model architecture | R2 |
| Derived weight artifact | future provider/model load | model load | Not in first executable plan | artifact/provider/version and loaded tensor map | R2/R3 |
| Multimodal projector/mmproj | model parameters and artifact | model load | Ornith stress only after proof | main/mmproj hashes and loaded placement | R2 |
| MTP inclusion/omission | converter/runtime-specific | model load/context | Stress-cell research | exact artifact and graph/operator report | R2 |
| `mmap`, direct IO, `mlock` | pinned model parameters/CLI | model load | Candidate after pinned audit | actual load policy and file handle identity | R2 |
| GPU layer count | pinned model parameters | model load | Candidate | actual device buffer map/bytes | R2 |
| Tensor buffer override | pinned model tensor buffer API | model load | Candidate after adapter proof | per-tensor actual buffer type | R2 |
| Context length | context parameters | context creation | Candidate | actual context/state allocations | R2 |
| Batch/ubatch | context/runtime API | context/request | Candidate after pinned audit | actual batch/ubatch | R1 or R2 by API |
| Generation/batch threads | llama context thread API where pinned | request/boundary | Candidate | actual thread counts and CPU-set resolution | R1 if backend supports boundary |
| CPU affinity/topology | native process/thread policy plus pinned runtime | process/request | Candidate | applied CPU Sets and failures | R1/R2 |
| KV K/V type | context parameters | context creation | Candidate for full-attention foundation | actual state type/bytes by architecture | R2 unless exact conversion exists |
| KV offload/placement | pinned context/backend control | context creation | Candidate after proof | actual buffer/state placement | R2 or proven R1 conversion |
| Flash attention on/off | context parameter/backend support | context creation | Candidate after pinned audit | actual attention path or unsupported reason | R2 |
| Kernel implementation | narrow GGML backend hook | operator | Optional Phase 8 | requested/actual variant and fallback reason | R0 |
| Pinned staging/prefetch | PrismInfer-owned pool and events | request/operator | Optional Phase 8 | slot/copy/consume event trace | R0/R1 |
| Draft model/device | upstream speculative parameters | model/context | Optional Phase 8 | draft artifact, placement, memory | R2 |
| Draft length/threshold | upstream request/runtime control | verification cycle | Optional Phase 8 | actual configuration and committed tokens | R1 after cycle |
| KV compression/retention | future state provider | token/block | Optional research | state type, conversion, bytes, quality policy | R1 only with exact conversion, else R3/R2 |
| Progressive precision tier | future derived provider | model/request/epoch | Static optional research first | actual streams/kernel/tier | R2/R3; no dynamic Phase 7 |
| Structured block mask | future graph/kernel/router | operator | Oracle only initially | router/provider/mask/confidence and state contract | R3 or pre-commit R0 |

## Phase 7 Executable Schema

The first executable plan is a **static acknowledged plan**. It may contain only
actuators proven at process, model-load, context-creation, or request setup:

```text
exact model/artifact cell
process and CPU topology policy
model-load file policy
GPU layer count and proven tensor overrides
context, batch, ubatch, and supported KV/context settings
static prefill/decode parameter choices that the pinned API can apply safely
measurement and fallback/restart contract
```

It does not contain per-epoch representation, tensor placement, KV format, or
structured-compute decisions.

Kernel dispatch, staging, KV transitions, progressive tiers, routers, and
speculative offload are independent optional providers admitted later.

## Acknowledgement Event

```text
event: actuator_acknowledgement
plan_id and entry_id
actuator_id and version
requested_value_hash
actual_value or actual_value_hash
applied_at_lifecycle
backend_commit and API/hook version
eligible: true|false
fallback_or_recovery_class
memory_effect_observed_or_pending
status: applied|defaulted|rejected|requires_restart
reason_code
```

A requested value without acknowledgement cannot support an implementation or
performance claim.

## State Commit Rules

Define commit points explicitly:

- operator output committed to graph state;
- KV/recurrent/convolution state committed;
- sampled token committed to the sequence;
- token/output released to the client;
- checkpoint released.

An R0 substitution occurs before the relevant commit. R1 occurs only when
state compatibility is proven. R2 stops continuity. R3 is either verified
before commit or explicitly approximate.

## Fault Tests

- Unsupported actuator in the plan.
- Pinned/master option mismatch.
- Backend reports a different actual value.
- Local kernel failure before commit.
- Partial staging/copy failure.
- KV/state conversion failure.
- Restart-required load-time mismatch during an active request.
- Router/audit failure before and after output commit.
- Fallback target absent or incompatible.
- Recovery memory exceeds cap.

The test passes only when the recorded recovery class matches actual behavior
and no seamless-reversal claim is made for R2/R3.
