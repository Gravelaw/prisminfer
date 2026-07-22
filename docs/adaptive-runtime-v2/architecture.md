# Adaptive Runtime V2 Architecture

## Architectural verdict

PrismInfer is a thin control and evidence layer with optional narrow providers.
Pinned llama.cpp/GGML is the provisional substrate hypothesis, not an already
proven provider platform. The current repository has a process-backed warmup
adapter and planning scaffolds; it does not yet have in-process lifecycle,
allocation governance, operator interception, or production GGML quantized
kernels.

The first architectural deliverable is therefore a seam-proof spike, not a new
codec or router.

## System boundary

~~~mermaid
flowchart LR
  R["Request and exact cell"] --> S["Trusted CPU-only supervisor"]
  S --> A["Two-stage admission"]
  A --> W["Contained native Job worker"]
  W --> L["Pinned libllama and GGML adapter"]
  L --> G["Default GGML CPU or GPU execution"]
  L -. "optional admitted seam" .-> B["Representation buffer provider"]
  B --> D["Decode GEMV provider"]
  B --> P["Prefill GEMM provider"]
  L -. "separate lifecycle" .-> K["KV and architecture-state provider"]
  S --> E["Append-only evidence and review receipts"]
  A --> E
  W --> E
  L --> E
  B --> E
  D --> E
  P --> E
  K --> E
~~~

The supervisor owns policy, admission, cancellation, and terminal evidence. The
worker owns the CUDA context, model/runtime objects, and provider calls. A
provider never obtains filesystem, network, process, background-thread, hidden
allocation, or implicit stream authority.

## Mandatory seam-proof spike

Before a provider ABI is frozen, a tiny exact fixture must demonstrate all of
the following against the pinned dependency:

- explicit model and context lifecycle without a shell;
- exact per-tensor `ggml_type` and artifact identity;
- buffer ownership plus persistent, temporary, workspace, and retained-pool
  accounting;
- requested and actual device/placement acknowledgement;
- actual operator and kernel-path trace;
- architecture-state creation, mutation, conversion, and destruction;
- caller-owned streams/events and bounded workspace;
- one default-safe public seam, extra buffer/backend path, or narrowly
  upstreamable dispatch hook;
- deterministic fallback to the upstream path.

Failure is retained evidence. It triggers a substrate review; it does not
automatically authorize a clean-sheet runtime.

The current baseline boundary in
[`src/backend/llama_backend.cpp`](../../src/backend/llama_backend.cpp) invokes
the approved native worker implemented by
[`src/runtime/native_worker.cpp`](../../src/runtime/native_worker.cpp). Packet B
replaced the former shell launch with `CreateProcessW`, Job containment, bounded
output/cancellation, and child-tree cleanup. The baseline still parses runtime
output, while
[`src/offload/offload_planner.cpp`](../../src/offload/offload_planner.cpp)
labels caller-supplied transfer fields. Those limitations are not actual-path
provider evidence.

The seam proof is an acceptance slice of Plan issue #85 in Packet D. Earlier
read-only source reconnaissance is allowed by its governing issue, but earns no
provider credit and cannot execute model work before the required clearance.

## Provider boundaries

### Offline representation compiler

Input is a pinned BF16/FP16 source or another explicitly approved parent
artifact. Output is a distinct immutable artifact with:

- source, recipe, converter, codebook, and output hashes;
- representation/version and dependency DAG;
- legal transform family, dimensions, seed/hash, absorption/inverse map, and
  online transform cost;
- tile shape, alignment, random-access granule, and maximum expansion;
- payload, metadata, index, padding, residual, codebook, checksum, and total
  bytes;
- decoded type/layout and canonical CPU decoder;
- supported operators, phases, devices, and provider versions.

### Representation buffer provider

This provider owns the compact resident form. Its acknowledgement must prove
that the ordinary source tensor is not also retained and erasing the capacity
benefit.

~~~text
supports(tensor_descriptor, artifact_descriptor)
query_resident_bytes()
query_persistent_bytes()
query_workspace(phase, operator, shape)
map_prevalidated(preopened_handle, caller_storage)
address_tile(tile_index)
validate_and_acknowledge()
~~~

### Phase-specific execution provider

Decode and prefill cannot share a generic profitability claim.

~~~text
query_support(operator, phase, m, n, k, representation, device)
query_workspace(operator, phase, shape)
launch_decode_gemv(caller_stream, caller_workspace, descriptors)
launch_prefill_gemm(caller_stream, caller_workspace, descriptors)
acknowledge(actual_variant, actual_workspace, actual_bytes, status)
~~~

Batch-1 decode usually rewards fused compressed fetch/decode in a GEMV-like
path. Prefill usually rewards a different decoded layout, bounded scratch, and
GEMM/Tensor-Core path. A bounded tile/tensor scratch buffer is admissible when
declared; full persistent high-precision model materialization is not.

### KV and architecture-state provider

KV/state has a mutable lifecycle and a different recovery contract from
immutable weights. It separately declares encoding, attention consumption,
residual window, conversion boundary, retained bytes, workspace, quality
fixtures, and fallback semantics. Ornith full-attention KV, DeltaNet/recurrent
state, convolution state, MTP, and optional multimodal state are separate
ledger entries.

## Runtime capability and comparison record

Issue #83 produces a dated `RuntimeCapabilityRecord` before any runtime feature
enters the actuator inventory. It records:

- exact runtime revision, supported OS execution mode, hardware backend, and
  build prerequisites;
- accepted artifact/quantization semantics and any required conversion;
- loading, placement, attention/KV, scheduling, speculation, kernel, and
  telemetry mechanisms actually present at that revision;
- observable controls, requested-versus-actual acknowledgement, allocation and
  path evidence, fallback, and recovery;
- maintenance status, license, and reproducibility constraints; and
- one role: within-cell baseline, paired-cell direct comparator, mechanism
  reference, conditional substrate candidate, orchestration layer, or UX
  wrapper.

A feature listed by another runtime or current upstream does not become a
PrismInfer actuator. Only a control implemented and acknowledged by the pinned
cell may enter the plan set.

## Integration ladder

Escalation is evidence-driven:

1. public llama.cpp/libllama controls and GGML observation;
2. extra GGML buffer type or backend registration;
3. narrow loader or operator-dispatch hook;
4. custom GGML backend for an admitted compact representation;
5. another mature runtime substrate;
6. clean-sheet runtime only after an architecture council reopens the decision.

Each step must show why the previous one cannot satisfy the exact capacity,
latency, acknowledgement, safety, and maintainability contract.

If the #85 GGML seam fails structurally, the retained failure report includes an
alternative-substrate scorecard covering native Windows viability, containment,
artifact and quantized-tensor equivalence, lifecycle/placement/KV control,
allocation and actual-path observability, deterministic fallback, maintenance,
licensing, and implementation cost. Each criterion is recorded as `pass`,
`fail`, `unknown`, or `not applicable`; no weighted total may hide a critical
failure. A failed seam opens a council decision; it does not select a runtime.
Only that council may create one bounded prototype issue using the same tiny seam
fixture and evidence contract.

## Runtime-decision reopen criteria

Reopen only when retained evidence demonstrates at least one structural failure:

- the pinned substrate cannot expose allocation truth, actual placement,
  architecture-state lifecycle, or actual-path acknowledgement;
- a useful representation requires persistent invasive changes across loader,
  graph scheduler, allocator, state lifecycle, and kernels instead of one narrow
  default-safe seam;
- GGML forces full reconstruction or extra transfers that erase the measured
  benefit;
- the pinned runtime cannot execute the foundation or Ornith stress cell
  correctly;
- the provider seam becomes unmaintainable across two consecutive pin updates;
- at least two independent providers encounter the same structural blocker and
  a narrow-hook design is unsuccessful or default-unsafe; or
- a bounded independent prototype passes the same 10/12 GiB cell, quality, and
  end-to-end threshold where the substrate is structurally unable to admit it.

A changed hardware hierarchy also permits review. One failed hook, one slow
kernel, or one unsupported model is insufficient. Another mature substrate is
evaluated before PrismInfer assumes ownership of a complete runtime.

## Plan and recovery lifecycle

The normative event sequence remains
[`../runtime-state-machine.md`](../runtime-state-machine.md). In summary:

1. validate exact cell and acquire the device lease;
2. perform pre-context admission;
3. create the suspended worker, assign the non-breakaway kill-on-close Job, and
   resume it;
4. create the minimum context and reconcile post-context capacity;
5. load one immutable, actuator-bound plan and reconcile actual runtime
   allocations and paths;
6. execute prefill/decode under the watchdog;
7. apply only the declared R0/R1/R2/R3 recovery;
8. reconcile cleanup and publish terminal evidence.

The provider is subordinate to this lifecycle. It cannot create a parallel
control plane.
