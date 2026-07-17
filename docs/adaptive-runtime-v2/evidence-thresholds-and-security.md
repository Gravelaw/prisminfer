# Evidence, Thresholds, Sampling, and Security

This document is the sole V2 owner of evidence entities, numeric thresholds,
sample-plan rules, artifact/provider trust, and reproducibility requirements.
Thresholds are PrismInfer continuation or stop policies. They are not constants
derived from the cited literature and must not be tuned after observing a
failed candidate. A threshold pass never grants Plan clearance.

## Evidence data model

Use linked, versioned entities instead of one mutable all-purpose observation:

- `ValidationCell`: exact hardware, software, model, artifact, workload,
  context, cap, phase, and policy identity;
- `ArtifactRecord`: source/derived hashes, parent DAG, recipe, representation,
  tokenizer/template, licensing, and trust state;
- `AdmissionReceipt`: pre-context and post-context inputs, reserves, cap,
  decision, and reason;
- `ActuatorDescriptor`: supported values, owner, apply boundary,
  acknowledgement, workspace, and recovery;
- `RawObservation`: append-only event with monotonic/wall time, source, unit,
  provenance, value/missingness, and correlation identifiers;
- `CalibrationTrial`: randomized trial order, partition, plan, raw-event hash,
  prediction, observation, and outcome;
- `CandidatePlan`: immutable requested actuators, predicted resources,
  feasibility result, and rationale;
- `CertifiedPlanBundle`: acknowledged plan, exact fingerprint, thresholds,
  calibration inputs, recovery graph, and review receipt;
- `RunEvidenceBundle`: raw events, summaries, timelines, profiler outputs,
  quality results, terminal state, and claim classification;
- `ReviewReceipt`: exact tree/artifact hashes, review tier, reviewer role,
  findings, disposition, and invalidation conditions.

Every numeric value carries:

~~~text
name
value or explicit missing state
unit
provenance: configured, predicted, observed, or inferred
source component and method
monotonic timestamp or observation window
uncertainty, precision, and aggregation
cell, run, request, plan, and event correlation ids
~~~

Raw events are append-only. A summary records the exact input hash and method.
Configured transfer bytes cannot be relabeled measured. Requested GPU layers
cannot be relabeled actual placement. A profiler claim retains the profiler
artifact.

The CPU-only Packet A evidence runner owns emission rather than trusting a
caller-supplied manifest. It generates the opaque run id, wall-clock start and
completion times, normalized records, evidence hash, deterministic fingerprint,
terminal sequence, and manifest. Versioned `evidence-policy-v1` and
`sample-plan-v1` fix the 15-minute publication window, five-minute future skew,
strict sequential counts, one final terminal record, and claim ceilings.
Completed runs require one or more exact `ok` trials; skipped, unsupported,
rejected, and aborted runs are terminal-only with a reason and rejected ceiling.

Publication uses a fresh exclusive directory, writes and flushes the manifest,
evidence, and digest files, writes `COMMIT.json` last, then performs an
exclusive no-replace rename. POSIX operations are relative to held directory
descriptors and use `renameat2(RENAME_NOREPLACE)`; Windows traversal and
publication use no-reparse NT handles, root-relative `NtCreateFile`, and
root-relative `NtSetInformationFile` rename. The deterministic fingerprint is
the final directory identity, so the same semantic input cannot publish twice
and an interrupted temporary directory does not burn a replay key. Consumers
enumerate and read only the five committed files
through held handles, verify every identity and hash, and distinguish retained
bundle integrity from the policy freshness required at publication.

## Hardware safety and admission thresholds

### T-100: GPU effective cap

Policy:

~~~text
policy_ceiling = 16 GiB
gpu_reserve =
  max(1 GiB,
      ceil(10% of pre-context live capacity),
      ceil(10% of post-context live capacity))
post-context effective cap =
  min(requested tier,
      admitted pre-context cap,
      post-context reserve-adjusted live cap)
~~~

Every lifecycle peak must stay within the effective cap. Reserve is never
payload. The run cap may shrink under the watchdog but cannot grow.

### T-101: host physical, commit, and pinned reserve

No fixed available-RAM prerequisite is valid. Workload-relative admission uses:

~~~text
development_nonpromotable physical reserve =
  max(2 GiB, ceil(8% of physical total))
development_nonpromotable commit reserve =
  max(2 GiB, ceil(5% of commit limit))

evidence_promotable physical reserve =
  max(4 GiB, ceil(15% of physical total))
evidence_promotable commit reserve =
  max(4 GiB,
      ceil(10% of commit limit),
      ceil(15% of physical total))

PrismInfer pinned-host cap =
  min(512 MiB, floor(2% of physical total))
~~~

Explicit reserves may raise but never lower these floors. Admit exact
incremental resident and commit peaks separately. Development evidence is
non-promotable. Pagefile/commit does not increase physical payload.

### T-102: thermal warning, stop, and restart

GPU warning is the lower of 78 C and eight degrees below a reported target.
GPU stop is the lowest available bound among 82 C, five degrees below target,
and five degrees below slowdown. For a materially participating CPU with an
approved package sensor, warn at the lower of 85 C and 15 degrees below
`TjMax`; stop at the lower of 90 C and 10 degrees below `TjMax`.

Warning forbids scale-up. Stop enters cancellation. Restart requires every
participating device at or below 70 C continuously for 60 seconds plus a fresh
preflight. Missing required current telemetry blocks C2+ except an explicitly
approved short attended cell proving that device is not a material load source.

### T-103: supervisor and heartbeat freshness

Supervisor sampling period is at most 100 ms. Worker heartbeat period is at most
250 ms. Required guard or heartbeat age is at most 500 ms. Missing or stale
evidence enters cancellation. Sampling remains outside the worker and records
drops and monotonic time.

### T-104: bounded dispatch

Before scale-up, every new GPU path must show measured single-dispatch p99 below
250 ms on the exact bounded fixture. No admitted dispatch declares a bound above
500 ms. Missing the gate requires split/tiled work. Timeout or device loss is
context-fatal and is never retried automatically. This is an engineering margin,
not a Windows TDR guarantee.

### T-105: cancellation and cleanup

On breach, block submissions immediately. Cooperative cancellation
acknowledgement is due within 500 ms; worker exit within 2 seconds; otherwise
terminate the Job. Cleanup and lease reconciliation finish within 5 seconds.
Automatic same-context retry count is zero. A missed deadline forces abort and
quarantine before another hardware run.

## Foundation thresholds

- **T-001:** observed GPU peak stays within T-100 for every lifecycle sample;
  requested tier is at most 16 GiB; promoted unknown GPU bytes equal zero.
- **T-002:** fixture pass rate is at least 95 percent, using a frozen paired,
  stratified binomial sample plan.
- **T-003:** task-quality regression is at most 5 percent, with per-stratum
  intervals and the worst stratum reported.
- **T-004:** warm decode median is at least 3 committed tokens per second.
- **T-005:** request ITL p95 is at most 750 ms.
- **T-006:** TTFT p95 is at most 30 seconds, with cold and warm cells separate.
- **T-007:** sustained decode coefficient of variation is at most 10 percent;
  this is a repeatability diagnostic.
- **T-008:** orchestration overhead is at most 2 percent in randomized paired
  same-plan comparisons.

T-004 through T-006 are exact foundation/stress usability policies, not
promises for every model or cap.

## Calibration and selector thresholds

- **T-020:** median held-out stage prediction error is at most 10 percent.
- **T-021:** held-out p95 stage prediction error is at most 20 percent.
- **T-022:** held-out end-to-end p95 prediction error is at most 10 percent
  before automatic selection.
- **T-023:** median committed-throughput regret is at most 5 percent versus the
  measured feasible oracle.
- **T-024:** tail regret is at most 10 percent versus the lowest-p95 feasible
  candidate.
- **T-025:** the promoted memory upper bound never underpredicts the observed
  promoted peak.

## Optional-mechanism thresholds

- **T-040:** a kernel or adaptive speed claim requires at least 1.10x
  end-to-end speed over the strongest same-cell upstream sweep, with the paired
  confidence interval excluding no improvement.
- **T-041:** transfer overlap continues only with at least 5 percent
  end-to-end phase benefit and retained serialized/overlapped timelines.
- **T-042:** mmap prefetch continues only with at least 10 percent cold-TTFT
  benefit and verified file/cache state.
- **T-043:** a lossless tile path requires at least 15 percent effective
  reduction and at least 10 percent exposed fetch-plus-decode benefit, followed
  by full-run confirmation.
- **T-044:** progressive weights require at least 15 percent effective bytes
  saved and at least 10 percent end-to-end benefit, with quality passing.
- **T-045:** KV quantization requires at least 40 percent net bytes saved and
  codec cost no more than 80 percent of measured time saving, plus quality,
  retrieval, negative-sample, and output-length gates.
- **T-046:** activation-transfer compression requires its timing inequality to
  pass with at least a 20 percent p95 margin on real boundary shapes and an
  end-to-end confirmation.
- **T-047:** the structured-compute oracle must remove at least 30 percent of
  hardware-aligned work at the quality limit before router training.
- **T-048:** a router must realize at least 95 percent of oracle savings and at
  least 10 percent end-to-end gain, including router/gather/audit overhead and
  out-of-distribution tests.
- **T-049:** a joint plan requires at least 10 percent over the best static
  same-cell baseline and may contain only independently passing mechanisms.

Interpretation:

- entropy paths count payload, metadata, indexes, padding, codebooks, checksums,
  workspace, and maximum expansion;
- decode and prefill pass independently;
- persistent full-tensor reconstruction invalidates a hot representation claim;
- capacity-only success is classified separately from speed;
- staging stops when exposed transfer wait remains or CPU execution is faster;
- this document does not authorize a joint trial; after C8 and refreshed #97
  admission, Packet G may run a joint candidate only when at least two
  mechanisms have independent full-pass receipts.

## Large-model viability thresholds

- **T-060:** committed target-distributed output throughput is at least 1 token
  per second.
- **T-061:** TTFT p95 is at most 120 seconds.
- **T-062:** ITL p95 is at most 2 seconds.
- **T-063:** throughput coefficient of variation is at most 10 percent.
- **T-064:** paired task-quality regression is at most 5 percent.
- **T-065:** memory, host, storage, actual-path, and claim-class evidence is
  complete.

These thresholds classify an already admitted exact scale cell. They do not
override a resource-DAG rejection.

## Sample-plan contract

Before measurement, freeze:

- hypothesis and exact primary/secondary endpoints;
- validation cell and strongest same-cell controls;
- independent sampling unit and stratification;
- search, calibration, validation, confirmation, and promotion partitions;
- sample size or precision target, alpha, interval method, and multiplicity;
- trial order/randomization, warm-up, cold/warm state, and cache policy;
- exclusion, retry, failure, missingness, and early-stop rules;
- paired estimator and request/session-level distribution to report;
- threshold-registry version and decision rule.

Tokens from one sequence, tiles from one tensor, or repeated timings inside one
process are not automatically independent. Use sessions/requests/artifacts as
the inferential unit where appropriate. Report raw counts, effect size,
interval, worst stratum, and all exclusions. Finalists receive a fresh
confirmation run. Sequential testing needs an alpha-spending or other
predeclared correction.

## Artifact trust

An immutable artifact record includes:

~~~text
source repository and immutable revision
license/access receipt
source model, tokenizer, template, config, mmproj, MTP, and imatrix hashes
converter, quantizer, transform, and provider commits
exact recipe and random seeds
per-tensor name, shape, type, offsets, lengths, and checksums
derived parent DAG and representation descriptor
payload, metadata, index, padding, residual, codebook, checksum, and total bytes
canonical CPU decoder and fixture hashes
approved roots and opened-handle identity
~~~

A derived lossy/progressive representation begins from the pinned BF16/FP16
source or an explicitly reviewed parent. It never silently requantizes an
existing lossy Q4 artifact. The source remains immutable and the derived output
has a new identity.

Before use:

- canonicalize and verify allowed roots;
- reject device paths, alternate data streams, unexpected reparse points, and
  paths outside policy;
- open once with the intended sharing/locking semantics and hash the bytes
  actually opened;
- validate magic, version, endian, layout, counts, offsets, lengths, alignment,
  dependency acyclicity, expansion bounds, and random-access indexes with
  overflow-safe arithmetic;
- enforce file, tensor, tile, metadata, and workspace maxima;
- fuzz parsers/decoders, use sanitizers, and differential-test the canonical CPU
  decoder before a GPU launch.

## Provider trust

An approved provider is versioned and hash-bound. It:

- declares exact operators, phases, shapes, dtypes, quant blocks, devices,
  representations, alignments, persistent bytes, workspace, and expansion;
- receives validated immutable descriptors and caller-owned stream/workspace;
- has no hidden allocation, filesystem, networking, process, or background
  authority;
- returns typed errors and actual-path acknowledgement;
- passes correctness, fuzz/sanitizer, cap, recovery, profiler, and full-model
  end-to-end gates;
- proves the source tensor is not retained alongside the compact form.

Unsigned or unregistered binaries, mutable search paths, ABI mismatches, missing
capability fields, and undeclared allocation fail closed.

## Privacy and retention

Default semantic capture is fixture-only. Production prompts, outputs, logits,
hidden states, activations, attention, KV/recurrent state, and router features
are not retained without explicit purpose, owner/consent/license, minimal field
set, access control, encryption, retention/deletion, redaction, and incident
policy. Telemetry uses identifiers and aggregates by default.

## Reproducibility bundle

Every promoted result retains:

- PrismInfer commit and dirty-tree patch hash;
- llama.cpp/GGML pin and complete build flags;
- compiler, CMake, CUDA, driver, library, OS, and provider versions;
- hardware/runtime fingerprint and power/thermal policy;
- all artifact, recipe, dataset, fixture, partition, and seed hashes;
- admission receipts, plan bundle, actuator acknowledgements, and recovery graph;
- raw observations, timelines, errors, profiler outputs, manifests, and
  comparator hashes;
- threshold/sample-plan version, decision, claim class, and known limits;
- exact review receipts and invalidation conditions.

A mutable branch name is not a pin. A changed covered path or artifact hash
invalidates its review receipt.

## M1 one-time artifact-production exception

On 2026-07-17 the repository owner authorized one bounded exception for Packet
A to acquire the already pinned Llama 3.1 8B Instruct source revision, produce
its canonical F16 intermediate and Q4_K_M GGUF, and perform the CPU/offline
artifact inspection needed by issues #74, #75, and #80. The retained receipt
must record the exact source files and hashes, llama.cpp revision, commands,
toolchain and host identity, sampled physical/commit memory, reserve-triggered
abort behavior, output hashes, and validation results.

This exception permits only the minimum download, conversion, CPU
quantization, hashing, inventory, and retained-slice work needed for Packet A.
It does not authorize CUDA, model inference, calibration, benchmarking,
self-hosted workflows, sustained hardware claims, or any performance or model
promotion. Missing telemetry, a physical or commit reserve breach, an
unexpected artifact identity, or a parser/provenance failure still stops the
operation and remains a valid negative result.

The local self-production attempts and the first remote CPU production job
preserve negative receipts: local runs stopped at the physical-memory reserve,
while the remote run completed quantization but could not persist its output
because the supplied Hub token lacked repository-write permission. The owner
therefore extended the same one-time exception to the directly related
`bartowski/Meta-Llama-3.1-8B-Instruct-GGUF` Q4_K_M object at producer revision
`bf5b95e96dac0462e2a09145ec66cae9a3f12067`. Admission requires its published
LFS SHA-256, complete local inventory, source-lineage metadata, retained
per-type differentials, and explicit third-party producer provenance to agree.
This transfer exception does not convert community provenance into
self-produced provenance and grants no inference, benchmark, CUDA, or
performance credit.

Artifact admission also binds the complete GGUF inventory, retained slices,
exact eligibility map, canonical Git-object oracle hashes, and the source hash
of the CPU differential decoder build. Hosted checks validate the checked-in
small-file provenance addresses without model bytes; local artifact admission
additionally opens the approved content-addressed GGUF and retained bytes
through held, no-reparse, single-link handles.

## Threshold change control

Freeze this registry for the first pilot. A later change requires a versioned
proposal, retained break-even evidence, impact analysis for prior results, and
the Plan/AGENTS review tier appropriate to the affected safety or claim
boundary. A threshold cannot be changed merely because a candidate failed.
