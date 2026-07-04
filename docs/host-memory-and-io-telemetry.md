# Host Memory and IO Telemetry

Host memory and storage pressure are part of constrained-inference evidence.
They must be recorded before a CPU-heavy or offload-heavy run can be promoted
beyond exploratory status.

## Required Phase 1 Minimum

For real backend claims, record at least:

- process working set or RSS equivalent,
- private bytes,
- commit charge where available,
- pagefile usage or commit pressure where available,
- free system RAM before and after the run,
- process IO read/write bytes,
- model storage path,
- mmap enabled/disabled,
- file-backed residency proxy when available,
- hard page faults or page-fault-rate proxy where available,
- cold-cache or warm-cache marker.

If a platform counter is unavailable, record the field as unavailable with a
reason. Do not silently omit it in benchmark evidence.

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

