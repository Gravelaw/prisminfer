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

The approved worker verifies its CUDA device against the supervisor-selected
DXGI LUID before announcing context readiness. It reports CUDA memory capacity,
then consumes the session token before allocating an exact post-admission
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
`review_status=pending-independent-review`.

## Authorization and promotion boundary

Do not dispatch this workflow from implementation review. A later authorization
packet must name the exact reviewed-main commit/tree, commands, worker and
workflow artifact identities, adapter/host, payload and time limits, required
WDDM/NVML/host/thermal sensors, attendance, abort conditions, and retained
outputs. Any commit/tree/content change invalidates the functional/safety review
receipt and the authorization packet.

After an explicitly authorized run, an independent reviewer must accept the
complete positive and negative receipt set before C2 may be proposed as closed.
A critical vulnerability, concrete safety-floor defect, missing telemetry,
identity mismatch, cleanup quarantine, or incomplete evidence blocks promotion
immediately. Model execution, downloads, calibration, and performance claims are
outside this lane and remain separately gated.
