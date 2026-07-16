# Host Memory and IO Telemetry

Host memory and storage pressure are part of constrained-inference evidence.
They must be recorded before a CPU-heavy or offload-heavy run can be promoted
beyond exploratory status.

Host admission is workload-relative. PrismInfer does not require a fixed
amount of free RAM such as 24 GiB. It retains a lane-specific safety reserve,
then admits only the exact planned incremental physical-resident and commit
peaks plus uncertainty against their separate live payloads.

## Authoritative System Counters

On Windows, obtain system-wide physical and commit counters from
`GetPerformanceInfo`/`PERFORMANCE_INFORMATION` and convert page counts with
checked arithmetic:

- `PhysicalTotal` and `PhysicalAvailable` define physical capacity and current
  availability;
- `CommitTotal` and `CommitLimit` define system commit charge and limit;
- commit headroom is checked `CommitLimit - CommitTotal`;
- `MEMORYSTATUSEX.ullTotalPageFile` and `ullAvailPageFile` are process-bounded
  and must not drive system admission.

Missing, stale, contradictory, mismatched, or overflowing required counters
fail closed. Pagefile/commit headroom is recorded independently and never
increases the physical-resident payload.

## T-101 Admission Lanes

The canonical formulas and change-control owner are in
[`Adaptive Runtime V2 evidence and thresholds`](adaptive-runtime-v2/evidence-thresholds-and-security.md#t-101-host-physical-commit-and-pinned-reserve).

- `development_nonpromotable` preserves a lower but nonzero physical and commit
  reserve so ordinary CPU-safe development can proceed under normal desktop
  usage. Its receipts can never be promoted.
- `evidence_promotable` preserves stricter physical and commit reserves and is
  required for evidence that may be promoted.
- explicit per-run reserves may raise, but never lower, the selected lane's
  floor;
- pinned host memory has a separate nonzero cap and is charged to both the
  planned resident and commit peaks;
- transition from development to evidence requires a fresh evidence-lane
  decision from fresh telemetry.

Admission is a decision about the exact next workload, not total model size or
nominal RAM. For example, 8-15 GiB available may admit a small bounded plan and
reject a larger plan; a 24 GiB incremental plan is rejected or downscaled
without disabling smaller development work.

## Required Phase 1 Minimum

For real backend claims, record at least:

- process working set or RSS equivalent,
- private bytes,
- commit charge where available,
- pagefile usage or commit pressure where available,
- physical total/available and system commit total/limit/headroom before and
  after the run,
- counter source, admission lane, lane reserves, exact planned resident/commit
  peaks, uncertainty, pinned bytes, payloads, and typed decision,
- process IO read/write bytes,
- model storage path,
- mmap enabled/disabled,
- file-backed residency proxy when available,
- hard page faults or page-fault-rate proxy where available,
- cold-cache or warm-cache marker.

Authoritative system physical total/available and commit total/limit/headroom
are mandatory for T-101 admission and promotable/C2+ evidence; if one is
unavailable, stale, contradictory, or non-authoritative, fail closed. Optional
process/pagefile/ETW diagnostics may be recorded as unavailable with a reason
when the applicable claim does not require them. Never silently omit a required
field.

## Phase 3 Offload Minimum

For offload profitability claims, also record:

- pinned host allocation current and peak bytes,
- staging buffer current and peak bytes,
- H2D and D2H bytes,
- H2D and D2H wait time,
- NVMe read/write bytes for NVMe policies,
- cold-cache and warm-cache runs separately,
- p50 and p95 token latency,
- time-to-first-token,
- sustained decode tokens/sec.

## Certification Rule

Moving pressure from VRAM to host RAM, pagefile, or NVMe does not make a run
validated. It only changes where the pressure must be measured.

Any unexplained host-memory or IO growth above the configured tolerance rejects
validated or deployable claims.

Development-lane output is exploratory/non-promotable even when all local
checks pass. A promotable result requires a fresh evidence-lane admission and
the remaining clearance, supervisor, artifact, and quality gates.

Issue #109 adds authoritative raw system-counter fields and the pure admission
decision. Issue #103 still owns supervisor integration and serialization of the
complete lane/reserve/peak/uncertainty/payload/token receipt; no current
manifest may be promoted as though that integration already exists.

