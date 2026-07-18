# Safety, Actuation, and Admission

This document owns the V2 actuation, acknowledgement, admission, and recovery
contract. [`AGENTS.md`](../../AGENTS.md) owns repository and hardware safety;
[`Plan.md`](../../Plan.md) owns clearances and execution order; numeric
thresholds are owned by
[`evidence-thresholds-and-security.md`](evidence-thresholds-and-security.md).

## Trust boundary

The outer supervisor is CPU-only and remains outside the CUDA/model worker. It
owns:

- exact-cell validation and admission;
- the per-device lease;
- native worker creation and Job assignment;
- independent guard telemetry and heartbeat monitoring;
- cancellation, termination, cleanup reconciliation, and terminal evidence.

The worker is created suspended with an explicit executable and encoded
arguments, assigned to a non-breakaway kill-on-close Windows Job, then resumed.
It receives a controlled environment and working directory and inherits only
approved handles. Shell launch, ambiguous executable resolution, and untracked
descendants are prohibited.

The worker owns the CUDA context, model/runtime objects, provider calls, and
bounded runtime telemetry. The worker cannot weaken or override the outer
supervisor.

## Two-stage admission

### Pre-context

Before worker model or CUDA work, the supervisor:

- resolves the physical device and policy ceiling;
- samples WDDM local budget/usage and required current safety sensors;
- records physical RAM and system commit headroom from authoritative APIs;
- computes GPU, pageable-host, pinned-host, commit, storage, and uncertainty
  bounds for the exact requested cell;
- rejects if required evidence is missing, contradictory, stale, or already
  outside reserve.

### Post-context

The worker creates only the minimum runtime context. The supervisor then:

- reconciles context/runtime bytes, CUDA free memory, and current WDDM state;
- recomputes an effective cap that may shrink but cannot grow during the run;
- checks the exact plan's weights, architecture state, workspace, pools,
  fragmentation, instrumentation, staging, and unknown-byte terms;
- admits model loading only when every tier remains within its independent
  reserve and threshold.

Requested tier, policy ceiling, physical capacity, WDDM budget, pre-context cap,
post-context cap, and observed peak remain distinct evidence fields. Reserve is
never payload. A requested 10 GiB, 12 GiB, or device-reference cell is not
permission to allocate that amount.

Pagefile and system commit do not create physical RAM. Mapped files describe
addressability, not residency. Pinned host memory is bounded transfer staging,
not a model-storage tier. Missing owned-allocation or residency evidence fails
closed.

## WDDM claim boundary

Under Windows WDDM, no single counter proves physical residency. Evidence must
separate:

- PrismInfer-owned allocation accounting;
- backend/runtime allocation and workspace accounting;
- CUDA free/total observations;
- DXGI local/non-local budget, usage, reservation, and availability;
- NVML corroboration;
- process/Job commit and host residency.

A promoted capacity claim identifies whether it proves owned allocations under
an effective cap or the stronger physical/no-oversubscription path. It never
uses NVML alone as process-residency proof.

## Actuator descriptor

Every controllable choice has an immutable descriptor:

~~~text
actuator_id and version
owner and lifecycle phase
supported values and exact units
public API, hook, or provider identity
apply boundary and restart requirement
requested-value field
actual-value acknowledgement field
memory/workspace query
state compatibility and conversion
fallback and recovery class
evidence and error schema
~~~

The controller generates only plans whose actuator descriptors are compatible
with the exact fingerprint. Unsupported, ignored, clamped, substituted, or
unacknowledged requests make the candidate infeasible.

## Static plan contract

The first controller plan is:

- session-static and immutable after admission;
- bound to exact artifact, representation, context, cap, runtime, provider,
  hardware, and calibration hashes;
- explicit about CPU/GPU placement, state formats, kernel/provider variants,
  workspaces, staging, and recovery edges;
- installed before the applicable load/context/request boundary;
- acknowledged by the component that actually owns each actuator.

Hot-path selection is O(1) table lookup or a bounded local state machine over
pre-certified choices. It does not search, allocate, compile, download, parse
new artifacts, or silently change representation.

## Requested-versus-actual acknowledgement

For every applied decision, retain:

~~~text
plan_id and actuator_id
requested value and unit
actual value and unit
owner component and code/provider version
apply timestamp and lifecycle boundary
actual operator/kernel/placement path
workspace and persistent-byte acknowledgement
status: applied, clamped, substituted, ignored, unsupported, or failed
reason and recovery edge
~~~

Only `applied` with an allowed exact value promotes automatically. An explicitly
declared equivalent substitution may be an R0 path. Configured or logged input
without owner acknowledgement is not observed execution.

## Recovery classes

### R0: local equivalent substitution

R0 occurs before operator, state, or output commit. It substitutes an
equivalent pre-certified implementation with compatible inputs, outputs,
workspace, and semantics. No already committed state is rewritten.

### R1: compatible boundary recovery

R1 occurs only at a declared boundary where state is already compatible or an
implemented exact conversion exists. The conversion's bytes, time, workspace,
and failure behavior are admitted and measured.

### R2: restart or reject

R2 cancels the worker/request and may restart from the beginning under a newly
admitted plan. It never claims seamless continuation. Unsafe pressure,
context/device loss, unknown bytes, or incompatible state normally requires R2.

### R3: explicit approximation contract

R3 either verifies the candidate before commit and falls back to an exact path,
or uses an explicitly authorized lossy state/output contract. After a token or
mutable state is committed, a later audit can quarantine future use but cannot
undo the released output or state transition.

Structured routing, lossy KV/state retention, approximate reconstruction, and
other semantic changes require R3; they cannot be mislabeled R0.

## Cancellation and cleanup

On a guard breach, the supervisor immediately blocks new submissions and
requests cooperative cancellation. If acknowledgement or worker exit misses
the registered deadline, it terminates the Job. Automatic retry in the same
context is prohibited. Cleanup reconciles:

- worker and descendant exit;
- Job accounting and handles;
- CUDA/runtime/provider allocations;
- device lease;
- artifact and temporary-file handles;
- terminal reason, last good sample, missed deadline, and evidence hashes.

Another hardware run requires a fresh preflight and the threshold-owned restart
conditions.

Packet B implements this boundary as supervisor-owned state rather than worker
advice. The contained-worker receipt is mandatory before post-context
admission; the exact Stage-B receipt and admission token are one-shot; watchdog
evaluation continuously reuses the authoritative #109 host physical/commit
decision. The Stage-A receipt fixes one absolute run deadline, the exact
workload-relative resident/commit/pinned host envelope, and the admitted
device-specific warning/stop temperatures; Stage B and the watchdog may only
retain or tighten those values. The one-shot token is also bound to the exact
root PID, Job identity, and approved executable identity. Owned-allocation
evidence requires an independently timestamped, process- and adapter-bound
WDDM/NVML measurement whose bytes exactly reconcile with supervisor-owned
categories; a generic telemetry-present flag has no evidentiary value. Any
stale, contradictory, resource, thermal, heartbeat, deadline,
or context-fatal sample immediately blocks submissions. Cancellation retains
the T-105 acknowledgement, exit, Job-abort, and cleanup deadlines, and cleanup
cannot release the lease as `Cleaned` when the Job tree or resources remain
unreconciled. Normal cleanup requires an observed worker exit and empty Job
tree plus reconciled Job accounting, device resources, artifact handles, and
temporary files. It retains the terminal reason and canonical hashes for the
last good sample and terminal evidence bundle. Any premature destruction or
unreconciled terminal path quarantines the adapter lease until the supervisor
process exits.

The native worker additionally retains approved read-only artifact root and
leaf handles without write/delete sharing from byte-hash verification through
complete Job-tree exit. The production llama adapter accepts only the Packet A
artifact record's exact filename, byte count, and SHA-256, plus a sidecar whose
digest is recomputed from the model bytes. This is still not C2 credit: the
session must own the live native-worker Job/process authority, cooperative
cancel signal, abort action, and OS-derived cleanup receipt rather than accept
caller-assembled containment or cleanup booleans. Until that end-to-end
lifecycle is implemented and reviewed, Packet B remains in Review and C2 stays
closed.

## Provider subordination

An optional provider:

- receives only validated descriptors, preopened immutable handles, caller
  streams/events, and preallocated memory;
- declares persistent, transient, and maximum expansion bytes before admission;
- cannot create hidden allocations, streams, processes, files, networking, or
  background work;
- acknowledges the actual representation and execution variant;
- returns a typed failure instead of aborting the host;
- follows the same R0-R3 and cleanup contract.

The normative event ordering and terminal states remain in
[`../runtime-state-machine.md`](../runtime-state-machine.md).
