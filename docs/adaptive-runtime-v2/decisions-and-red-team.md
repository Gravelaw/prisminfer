# Decisions and Red-Team Record

This is a historical rationale record. It is explicitly non-normative. The
authority map in [`README.md`](README.md) identifies the binding owner for
every decision.

## Council composition

The virtual council was modeled on the role families in the supplied
OpenAI/Anthropic and llama.cpp contributor background research:

- **Research scientist:** applied mathematics, information theory, statistics,
  ML experimentation, publication-quality falsification, and quality metrics.
  This reflects scientist profiles where original research evidence matters.
- **GPU/runtime performance engineer:** modern C++, CUDA, SIMD/SIMT, quantized
  kernels, memory hierarchy, profiling, GGML/GGUF, and upstream runtime
  maintenance. This reflects inference/accelerator roles and llama.cpp
  contributor paths, including systems engineering and physics-derived
  numerical work.
- **Evidence and infrastructure engineer:** Windows process/Job security,
  WDDM/DXGI, allocator/telemetry truth, schemas, reproducibility, CI, and
  operational failure handling.
- **Devil's advocate:** adversarial novelty, substrate, mathematics,
  falsifiability, milestone, and documentation-sprawl review.
- **Systems architect:** final component boundaries, authority ownership,
  integration ladder, and runtime reopen policy.
- **Planner:** one-to-one mapping from the root Plan packets to outcomes,
  artifacts, prohibitions, reviews, and valid negative results.

These are review lenses, not claims that named employees or contributors
participated.

## Inputs from the earlier discussion

The two supplied videos were treated as explanatory prompts:

- [entropy and compression video](https://youtu.be/l6DKRf-fAAM?si=NtbryrCJhp0u6k7T);
- [cross-entropy video](https://youtu.be/GlYgs6v2YfU?si=2nV6NRNeNS__f88g).

They motivated three questions: whether tensor streams remain compressible after
quantization; whether predictive cross-entropy can guide runtime choices; and
whether changing vector-coordinate directions can reduce quantization
distortion. Binding theory was taken from primary information-theory,
rotation/quantization, compression, and systems sources in
[`research-hypotheses-and-references.md`](research-hypotheses-and-references.md).

Council conclusion:

- token cross-entropy, tensor-symbol entropy, and operational effective bytes
  answer different questions;
- an orthogonal rotation preserves joint information but may redistribute
  outliers for a constrained quantizer;
- compression is useful only when its representation is directly consumable or
  its bounded decode sits off the exposed critical path;
- vector direction/rotation is a representation compiler concern, not an
  arbitrary runtime transform across nonlinear/state boundaries.

## Council verdict

The council reached a conditional go:

1. remain over pinned llama.cpp/GGML by default;
2. call that substrate provisional until a seam-proof spike passes;
3. narrow the research thesis to directly executable hot base, bounded residual
   prefixes, selective cold entropy, exact-cap static placement, and
   phase-specific providers;
4. run Llama 3.1 8B first and Ornith 9B second;
5. place provider credit after exact 30B static truth;
6. retain negative/rejected outcomes as first-class results.

No member supported a clean-sheet runtime as the immediate direction.

## Devil's-advocate objections and dispositions

### Objection 1: the provider boundary was hypothetical

Evidence at the council freeze: the adapter shelled out via `std::system` and
scraped logs. Packet B later replaced that launch path with the contained native
worker, but the baseline still parses runtime output; the offload planner
accepts caller-fed byte values, and the repository does not yet prove
in-process GGML buffer, placement, operator, or state control.

Disposition: **accepted**. V2 calls GGML a provisional substrate and makes the
tiny exact seam proof an explicit #85/Packet D acceptance slice. A failed seam
triggers the escalation ladder rather than silently freezing an ABI.

### Objection 2: the novelty claim was occupied

Evidence: HyperQuant, ZipServ, QuaRot, SpinQuant, QuIP#, HIGGS, MatGPTQ,
Any-Precision, BitStack, DecDEC, Lynx, and related work cover the component
ideas.

Disposition: **accepted**. V2 makes no novelty claim for entropy plus rotation,
vector/lattice quantization, progressive weights, compressed KV, speculation,
or fused decode. Only an exact consumer-Windows/GGML composition result or
negative systems bound may become a later contribution.

### Objection 3: V1 mathematics contained invalid or ambiguous expressions

Disposition: **accepted and corrected**:

- operational effective rate multiplies total bytes by eight;
- representation selection and residency carry matching indexes;
- resource demand and capacity use the same rate units;
- the normative objective is feasible-set-first and lexicographic/Pareto;
- quality promotion uses a one-sided binomial upper bound;
- speculation uses pooled committed-output ratios;
- structured compute uses a sequential residual recurrence;
- offline immutable-weight encode cost is separated from online break-even;
- rotation preserves joint differential entropy.

### Objection 4: Packet A and the safety-first critical path appeared to conflict

Disposition: **accepted without changing the Plan**. V2 follows the Plan's
conservative A-through-G packet order, keeps Packet A CPU/offline only, requires
Packet B before model execution, and begins Packet C only after both exits.

### Objection 5: nineteen proposed documents would repeat V1's drift

Disposition: **accepted**. The architect reduced V2 to nine canonical files.
Formulas, thresholds, recovery classes, packet order, and status each have one
owner.

### Objection 6: early representation work could bypass #94

Disposition: **accepted**. Packet A may retain a non-credit offline census after
lawful artifact availability. It cannot produce provider, model, capacity,
quality, speed, or novelty credit. Runtime representation work remains #94.

## 2026-07-29 research refresh and second red-team

Issue #130 repeated the council review against primary papers, official runtime
documentation, SHA-pinned source snapshots, the current repository baseline,
and a separate devil's-advocate audit. The overall decision remains **keep**:
PrismInfer should continue as a thin safety, control, evidence, and optional
provider layer over the provisional pinned llama.cpp/GGML substrate. A
clean-sheet runtime remains rejected unless #85 proves a structural seam failure
and the architecture reopen criteria pass.

The mechanism dispositions became narrower:

| Mechanism | Disposition | Consequence |
|---|---|---|
| KV quantization and placement | **Keep, split evidence** | Quantization, retention/eviction, host placement, and hierarchical reuse receive separate byte, quality, response-length, transfer, and phase records. DirectKV's NVLink-C2C result and Strata's long-context SGLang result do not transfer to PCIe/WDDM. |
| Fixed hot base plus residual weights | **Revise** | DecDEC, Any-Precision, BitStack, MatGPTQ, and related work occupy the component space. Continue only from an approved source artifact with direct-consumption kernels, random access, full effective-rate accounting, and fixed-quantization controls. |
| Activation-transfer compression | **Reject for the current default single-GPU cell** | Preserve the hypothesis but do not implement it until profiling shows the uncompressed boundary is material. T-046 now requires at least 10 percent exposed phase time before entry and at least 5 percent end-to-end phase benefit. |
| Structured-compute oracle and router | **Revise** | Keep the oracle as a falsifier. Predictor error, batching, sparse-kernel availability, gather/scatter, and dense fallback are charged. Oracle success only permits router construction; the guarded router requires a separate full-model pass. |
| Committed-output-aware speculative offload | **Reject as an implementation until specified** | Keep committed-output and wasted-work accounting as mandatory comparator semantics. #93 cannot implement until it freezes a concrete exact-target-distribution algorithm and a draft/verification/memory/transfer roofline predicting material end-to-end benefit. |
| Custom kernel dispatch and staging | **Revise** | ADAngel and current upstream dispatch support an offline phase/shape/bit-width policy map, not one universal kernel. Kernel semantics and staging/overlap require independent receipts or a predeclared factorial design. |
| Joint optimization | **Revise and defer** | At least two compatible independent passes remain necessary but not sufficient. T-049 now compares with the best static, single-component, and eligible lower-order controls and requires a dependency/overlap/recovery/combined-resource record. |

The red-team also accepted four protocol objections:

- a point estimate or confidence interval that merely excludes no improvement
  cannot satisfy a larger policy margin;
- candidate search, phases, seeds, endpoints, and repeated confirmation create
  one multiplicity family rather than independent chances to pass;
- every passing mechanism/phase needs its own locked full-model confirmation
  against the hash-bound #90 and strongest eligible upstream controls; and
- `pass-promotable`, `complete-nonpromotable`, `not-admitted`, and
  `inconclusive` are different outcome states; an `inconclusive` outcome is
  retained but cannot close a dependency.

The binding corrections are owned by
[`evidence-thresholds-and-security.md`](evidence-thresholds-and-security.md) and
[`major-milestones.md`](major-milestones.md). They change no current clearance,
issue state, implementation fact, or C2 status.

## Architecture decision summaries

### ADR-001: one planning coordinate system

Use root Plan clearances and packets only. Remove V1 P7/P8/P9, E0-E10, and the
alternate clearance ladder as governing coordinates. Do not copy live issue
status.

### ADR-002: provisional llama.cpp/GGML substrate

Default to public controls, then a narrow extra-buffer/backend or dispatch seam.
Escalate to another mature runtime before a clean-sheet runtime. Reopen only
with the structural evidence defined in
[`architecture.md`](architecture.md).

### ADR-003: representation and kernel are one measured contract

Nominal compression is insufficient. The artifact, buffer residency, maximum
expansion, workspace, random access, decode/prefill/KV path, source duplication,
quality, and full-model replay are one evidence cell.

### ADR-004: phase-specific providers

Decode GEMV, prefill GEMM, and KV/architecture-state consumption pass
independently. A generic provider launch or one microkernel win cannot promote
all phases.

### ADR-005: exact artifact identity precedes representation selection

The static controller holds representation fixed. Derived representations start
from pinned BF16/FP16 or an approved parent and create new artifact/provider
cells.

### ADR-006: model order and cap tiers

Llama 3.1 8B is first; Ornith 9B is a separate hybrid-state stress cell.
Requested 10 GiB and 12 GiB tiers are primary; 8 GiB is stress-only; the
physical-device reference is separately measured and never rounded upward.

### ADR-007: independent mechanisms before joint planning

Kernel/staging, KV/state, speculation, representation, and structured compute
receive independent decisions. At least two full passes are required before a
joint candidate becomes eligible. Plan keeps any joint admission, trial, and
manifest in Packet G after C8 and refreshed #97 admission.

### ADR-008: nine single-owner documents

V2 keeps authority, program, architecture, mathematics, safety, evidence,
research, milestones, and historical decisions separate. Historical council
prose cannot override a contract.

### ADR-009: archive is a validated move

Create and validate V2 beside V1, migrate active links, move V1 atomically to
the archive, leave archived files unchanged, and verify that no active document
points to the old path.

## Unresolved hypotheses

- Whether the pinned llama.cpp/GGML revision exposes a narrow maintainable seam.
- Whether Llama 3.1 8B and Ornith 9B artifacts are accessible, supported, and
  admitted under their exact manifests.
- Whether a fixed hot base plus residuals wins after full effective-rate and
  runtime costs.
- Whether entropy remains profitable on any cold residual stream.
- Whether rotations and activation-selected residual routing are compatible.
- Whether any optional mechanism beats the strongest upstream same-cell sweep.
- Whether 30B, 70B, or 90B exact cells are usable rather than merely storable.

The milestone program exists to answer these questions, not assume them.
