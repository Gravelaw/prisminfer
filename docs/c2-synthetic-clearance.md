# Synthetic C2 clearance candidate lane

This lane prepares evidence for the C2 hardware-safety prerequisite after the
completed `#103` supervisor/admission fault suite. It does not itself grant C2.
Code review, hosted tests, CPU simulations, a workflow definition, and an
unreviewed hardware artifact all remain non-promotable and carry zero C2 credit.

## Legacy workflow audit

The existing `cuda-probe-self-hosted.yml`, `cuda-kernel-self-hosted.yml`, and
`offload-profitability-self-hosted.yml` lanes predate the integrated Packet B
supervisor contract. They do not demonstrate one exact worker identity moving
through `CONTEXT_READY`, admission-token consumption, bounded heartbeats,
cooperative cancellation, Job containment, exclusive device ownership, and
final cleanup reconciliation. Their retained results remain useful historical
smoke/correctness/profitability evidence, but are non-promotable for C2 and do
not close it.

## Candidate contract

`c2-clearance-self-hosted.yml` is manual-only on reviewed `main`, has read-only
repository permissions, checks the exact reviewed commit and source tree, and
requires an attended one-shot authorization identifier. Repository concurrency
serializes its GPU workflows. `run-c2-clearance.ps1` additionally holds a
non-waiting host-wide Windows mutex while the production `GpuAdmissionSession`
holds the exclusive adapter-LUID lease for each worker case.

The approved worker verifies its CUDA device against both the
supervisor-selected DXGI LUID and the authorization-bound NVML/CUDA GPU UUID
before announcing context readiness. Thermal samples resolve their NVML handle
by that UUID rather than a numeric index.

The clearance lane rejects every independent process-memory sample unless its
source is exactly the native `wddm-process` counter; the general runtime's NVML
process fallback remains useful outside C2 but cannot satisfy this evidence
class, and its observed source/rejection is retained in terminal evidence. The
worker reports CUDA memory capacity, then consumes the session token before
allocating an exact post-admission
payload of no more than 64 MiB. Each case has a 10-second worker timeout, no
automatic retry, bounded Job containment, protocol cancellation, and explicit
cleanup. The four cases are:

- success;
- post-context telemetry loss;
- heartbeat loss; and
- watchdog cancellation.

The v1 receipt binds the reviewed commit/tree, worker SHA-256 and approval
identity, adapter LUID/index, host workflow run, lease/Job identity, payload,
CUDA context/final observations, heartbeat count, last-good/evidence hashes,
and cleanup reconciliation. Every emitted receipt hard-codes
`promotable=false`, `c2_credit=false`, and
`review_status=pending-independent-review`. Cleanup additionally rejects a
positive final WDDM local-usage delta above the versioned 16 MiB noise bound;
the pre/final counters, actual positive delta, and bound are retained.

## Authorization and promotion boundary

Do not dispatch this workflow from implementation review. A later authorization
packet must name the exact reviewed-main commit/tree, commands, worker and
workflow artifact identities, adapter/host, payload and time limits, required
WDDM/NVML/host/thermal sensors, attendance, abort conditions, and retained
outputs. Any commit/tree/content change invalidates the functional/safety review
receipt and the authorization packet.

Each candidate receipt retains the actual last global-WDDM, process-WDDM,
host, and thermal availability, timestamps, and guard values, including the
process-bound current/peak bytes and source. A canonical last-good hash is
sensitive to those safety fields and always selects the most recent accepted
guard sample: pre-context for post-context loss, post-context for watchdog
cancellation, and the last accepted watchdog sample otherwise. The failing or
missing guard is retained separately as canonical terminal-trigger material
with its own verified hash. A separate pre-cleanup hash is passed into
the session cleanup gate, while the terminal bundle hash additionally binds the
observed failure, Job/handle/file reconciliation, final WDDM delta, and actual
cleaned/quarantined result. Receipt validation recomputes both canonical hashes;
changing a safety or cleanup field without rehashing is rejected.

After an explicitly authorized run, an independent reviewer must accept the
complete positive and negative receipt set before C2 may be proposed as closed.
A critical vulnerability, concrete safety-floor defect, missing telemetry,
identity mismatch, cleanup quarantine, or incomplete evidence blocks promotion
immediately. Model execution, downloads, calibration, and performance claims are
outside this lane and remain separately gated.
