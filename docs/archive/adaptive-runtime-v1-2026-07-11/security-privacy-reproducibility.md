# Security, Privacy, Authenticity, and Reproducibility Contract

Status: normative design contract for adaptive-runtime implementation and
evidence.

## Assets and Trust Boundaries

Protected assets:

- model, tokenizer, mmproj/MTP, imatrix, derived representation, router, plan,
  kernel/provider, fixture, and calibration identities;
- native process and dynamic-library execution boundary;
- GPU/CPU memory safety and cap evidence;
- prompts, tokens, logits, hidden states, KV/state, and activation traces;
- telemetry, manifests, profiler traces, and GitHub/self-hosted runner state;
- reproducibility and claim classification.

Potentially untrusted inputs:

- GGUF and sidecar files;
- derived compressed/progressive artifacts;
- plan bundles and fallback graphs;
- prompt/fixture files;
- executable/provider/DLL paths;
- environment variables and working directories;
- metadata dimensions, offsets, codebooks, residual dependency graphs, and
  compressed tile lengths;
- external process output/logs;
- CI pull-request code on hardware runners.

## Threat Model

| Threat | Example impact | Required control |
|---|---|---|
| Shell/argument injection | Model path or prompt executes commands. | No shell execution; native argv/process API and strict argument builder. |
| Path traversal/reparse/TOCTOU | Approved path swaps to another file/DLL. | Approved roots, canonical/reparse checks, open-handle identity, hash after open. |
| Plan substitution | Attacker selects unsafe kernel/artifact or weak cap. | Signed/approved plan identity, compatibility checks, immutable hash and trust record. |
| Malformed GGUF/derived artifact | Overflow, OOB read/write, GPU corruption. | Strict format/schema, size/count/range checks, overflow-safe arithmetic, CPU validation before GPU. |
| Decompression bomb | Tiny index expands beyond host/VRAM cap. | Per-tile and total expansion limits, workspace admission, streaming bounds. |
| Provider/DLL loading | Arbitrary native code. | No arbitrary provider paths in plans; approved provider registry and trust root. |
| Telemetry tampering | False cap/speed/quality result. | Source provenance, append-only run identity, hashes, cross-source reconciliation. |
| Prompt/activation retention | Sensitive user data leaks through traces. | Fixture-only default, explicit opt-in, minimal identifiers/aggregates, retention/delete policy. |
| Self-hosted runner compromise | PR executes arbitrary code on GPU workstation. | Manual/trusted dispatch, no untrusted PR code, least-privilege credentials, artifact isolation. |
| Calibration poisoning | Malicious/misleading trials select unsafe plan. | Approved fixture/artifact sources, raw sample retention, sealed confirmation, plan approval. |

## Hardware Supervisor and Staged Admission Contract

Issue #103 is the sole hardware-clearance owner for model-backed execution. A
trusted outer process must acquire an exclusive GPU lease and perform a
conservative read-only preflight before it creates the contained worker. The
worker may initialize only the CUDA context; the supervisor then reconciles the
actual context footprint and live budget before issuing an exact, bounded,
one-shot workload token.

The out-of-process watchdog monitors wall time, owned allocations, effective
GPU budget, T-101 lane-specific physical/commit payload, temperature/power
evidence, worker liveness and sensor health. Missing or contradictory mandatory
evidence, budget loss, deadline,
thermal threshold, worker crash or device-fatal error initiates bounded cancel
then Job termination. Evidence is published atomically as complete or aborted;
partial output cannot be promoted. This gate is tested with arithmetic
boundaries, sensor loss, admission rejection, worker crash, timeout, budget
drop, thermal trip and cleanup fault injection before any 8B/9B calibration.
Host admission uses exact incremental resident/commit peaks and authoritative
system counters; no fixed free-RAM prerequisite is accepted, and development
lane receipts are always non-promotable.

## Native Process Launch Contract

Replace the current shell-based launcher before a plan can provide paths or
runtime parameters.

Required Windows implementation:

- `CreateProcessW` or an equivalently safe native API;
- explicit canonical executable path;
- Windows-correct argument quoting with no `cmd.exe`, `call`, redirection, or
  shell metacharacter interpretation;
- controlled environment allowlist and working directory;
- restricted handle inheritance and pre-created stdout/stderr handles;
- process/job handles retained from creation;
- Job Object assignment, lifetime, memory/IO observation, timeout and cleanup;
- securely generated opaque log/artifact names;
- schema-safe run ID independent of filesystem paths.

Test shell characters, quotes, percent/variable expansion syntax, Unicode,
spaces, device names, long paths, and failure cleanup.

## Artifact Trust and Authenticity

Integrity and authenticity are separate:

- SHA-256 proves bytes match a recorded value.
- Authenticity/approval proves which publisher/operator accepted those bytes
  for this project.

Every executable artifact record includes:

```text
artifact_type and schema/format version
source repository and immutable revision
publisher/owner
approval identity and timestamp
expected size/count bounds
SHA-256 and optional signature
approved root and open-handle file identity
license/use conditions
converter/compiler/provider identity and options
parent/source hashes and dependency graph
revocation status
```

A plan references registry IDs and hashes. It cannot introduce an arbitrary
executable, DLL, kernel binary, decoder, router, or provider path.

## Path and File Handling

- Resolve only under configured approved roots.
- Normalize and validate before opening, then retain the open handle and verify
  final file identity/reparse status to prevent swap races.
- Reject device paths, alternate data streams, unexpected reparse points, and
  paths outside policy.
- Enforce maximum artifact, tensor, metadata, index, and file counts.
- Use read-only sharing/locking policy appropriate to immutable evidence.
- Hash the bytes actually opened, not a path later reopened by a different
  component.
- Separate source, derived, cache, temporary, and output directories.

## Decoder and Provider Safety

Before a compressed/progressive artifact reaches GPU code:

- validate magic/version/endian and canonical layout;
- use overflow-safe offset/length/count arithmetic;
- ensure ranges stay inside the opened file;
- enforce block/tile and total expansion ratios;
- validate codebook/residual dependency acyclicity;
- validate alignment/padding and representation eligibility;
- checksum independently addressable tiles when the format supports it;
- query workspace and admit it under the cap;
- decode adversarial tiles with the CPU reference;
- fuzz parser/decoder and run sanitizers;
- fail before GPU launch on any structural error.

The unchecked `tier_total()` addition in the current KV ledger must be replaced
with overflow-safe arithmetic before artifact-derived sizes use it.

## Experimental Kernel Provider ABI

C++20/CUDA remains the runtime. Optional CUDA, CUTLASS, Triton-generated, Mojo,
or other experiments must use a narrow versioned provider boundary rather than
becoming another runtime.

Conceptual C ABI:

```text
prisminfer_kernel_provider_v1
  abi_version
  provider_id/version/build_hash
  enumerate_capabilities()
  supports(operator_descriptor)
  query_workspace(operator_descriptor)
  launch(operator_descriptor,
         immutable tensor descriptors,
         stream_handle,
         preallocated workspace,
         error_out)
  synchronize_or_acknowledge()
```

Rules:

- explicit sizes, strides, dtypes, quant blocks, alignments and device identity;
- no hidden allocation or stream creation;
- workspace queried and preallocated;
- no filesystem/network access from launch;
- errors returned without aborting the host;
- provider loaded only from the approved registry;
- exact correctness, memory, profiler, and end-to-end gates before retention.

Mojo's current C ABI/FFI capability makes such an isolated experiment possible,
but native Windows and systems-runtime maturity still exclude it from owning the
main runtime.

## Privacy and Data Retention

Default mode is fixture-only capture.

Production prompts, generated tokens, logits, hidden states, activations,
attention maps, KV/recurrent state, and router features are not retained unless
all of these are declared:

- explicit opt-in and purpose;
- data owner/source/license/consent;
- minimal field set;
- access control and encryption at rest/in transit;
- retention duration;
- deletion path and audit;
- whether data enters calibration/training;
- redaction or pseudonymization policy;
- incident/revocation handling.

Telemetry uses run/model/plan/fixture identifiers and aggregates by default.
Raw prompt/output content is not necessary for memory or timing evidence.

Router and quality datasets use approved fixtures with split provenance. An
activation trace is a sensitive derived artifact and follows the same registry
and retention controls as a model artifact.

## Reproducibility Bundle

Every promoted experiment retains:

```text
PrismInfer commit and dirty-tree patch hash
llama.cpp/GGML commit and complete build flags
compiler, CMake, CUDA, driver, library and OS versions
hardware/runtime fingerprint and power/thermal policy
HF/source repository immutable revision
main model, tokenizer, template, mmproj, MTP, imatrix and derived hashes
converter/quantizer/provider commits and exact options
dataset/fixture revisions, license, split assignment and split seed
calibration/search/validation/confirmation/promotion partitions
trial randomization seed/order, warm-up, stop rule and threshold registry version
plan bundle, actuator inventory, fallback/recovery graph and hashes
raw observations, errors, telemetry, manifests, comparator and profiler hashes
claim classification and known limits
```

A mutable `main` reference is not a pin.

## Session Input Preservation

`session-thesis-and-evidence-map.md` is explicitly a structured reconstruction,
not a verbatim transcript. It must not be described as the complete raw
conversation.

For the local research note `D:\Research\Speculative Inferencing.md`:

- record its SHA-256 in the council evidence;
- copy a reviewed snapshot into a versioned research-input location before
  publishing docs, or store it in an approved artifact registry;
- reference the portable snapshot/registry identity rather than relying only on
  an absolute local path;
- preserve its original encoding/content and record any normalization.

## Self-Hosted GPU Runner Policy

- Manual or trusted-branch dispatch only.
- No pull-request code from untrusted forks/users.
- Minimal GitHub token and no unrelated secrets.
- Isolated working directory and artifact roots.
- Clean/verify runner state between jobs.
- Pin action SHAs and toolchain/provider versions.
- Upload only approved evidence paths; scan for prompts/secrets.
- Retain workflow/run identity in the manifest.

## Adversarial Tests

- Shell metacharacters and malformed Windows quoting.
- Path traversal, symlink/reparse swap, alternate data stream, device path.
- Huge/negative/overflowing dimensions and offsets.
- Truncated GGUF, index, residual or codebook.
- Cyclic/duplicate residual dependencies.
- Decompression expansion beyond declared maximum.
- Provider with hidden allocation, wrong workspace, invalid stream, crash.
- Plan substitution, wrong source parent, revoked artifact.
- Telemetry file modification/missing sequence/duplicate run ID.
- Raw prompt or activation written in default privacy mode.
- Self-hosted workflow triggered from an untrusted PR.
- Dirty tree without retained patch hash.

## Promotion Gate

No plan-driven execution or derived provider is promoted until:

- native process and artifact boundaries pass their adversarial tests;
- every executable artifact has integrity and approval records;
- plan and provider cannot load arbitrary code/paths;
- parser/decoder arithmetic and expansion are bounded;
- privacy defaults retain no production semantic data;
- reproducibility bundle is complete; and
- evidence tampering or missing provenance causes rejection/downgrade.
