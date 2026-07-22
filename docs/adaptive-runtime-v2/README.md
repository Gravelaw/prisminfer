# PrismInfer Adaptive Runtime V2

Status: active program specification on M0 merge, with the #125 runtime
comparator and exact service-cell contract aligned on 2026-07-22.

This package replaces the first adaptive-runtime research packet. It does not
grant CUDA, model, calibration, benchmark, download, workflow, or implementation
clearance. The root [`Plan.md`](../../Plan.md) remains the only authority for
program thesis, scope, issue order, clearances, active status, phase exit, and
packet execution.

## Decision

PrismInfer should remain a thin control, evidence, representation, and optional
provider layer over a pinned llama.cpp/GGML substrate while that integration
hypothesis is tested. A clean-sheet runtime is neither authorized nor ruled out
forever. It may be reconsidered only through the evidence gates in
[`architecture.md`](architecture.md).

The research hypothesis is deliberately narrower than the original proposal:

> A fixed-rate directly executable hot base, tile-addressable residual prefixes,
> entropy coding only on demonstrably skewed cold residuals, cap-aware
> session-static placement, and phase-specific fused providers may improve an
> exact consumer-Windows inference cell after all metadata, workspace,
> conversion, transfer, quality, and safety costs are counted.

This is a falsifiable composition hypothesis, not a novelty or performance
claim. Rotation, lattice/vector quantization, entropy coding, progressive
precision, compressed KV, and fused compressed execution all have substantial
prior art.

## Authority and ownership

| Subject | Sole authority |
|---|---|
| Repository safety and operations | [`AGENTS.md`](../../AGENTS.md) |
| Program thesis, scope, cells, claim classes, issue dependencies, packet order, clearances, and phase exit | [`Plan.md`](../../Plan.md); GitHub Project 2 mirrors live status |
| V2 research questions and falsifiable working hypothesis within Plan scope | [`program-charter.md`](program-charter.md) |
| Runtime boundary, capability record, provider seams, conditional substrate scorecard, and escalation | [`architecture.md`](architecture.md) |
| Mathematical notation, optimization, and statistics | [`optimizer-mathematics.md`](optimizer-mathematics.md) |
| Admission, actuation, acknowledgement, and recovery | [`safety-actuation-and-admission.md`](safety-actuation-and-admission.md) |
| Evidence schema, thresholds, sampling, and provider trust | [`evidence-thresholds-and-security.md`](evidence-thresholds-and-security.md) |
| Research hypotheses, novelty boundaries, and sources | [`research-hypotheses-and-references.md`](research-hypotheses-and-references.md) |
| Milestone outcomes and acceptance artifacts | [`major-milestones.md`](major-milestones.md) |
| Council history, red-team disposition, and ADR summaries | [`decisions-and-red-team.md`](decisions-and-red-team.md), non-normative |

No V2 document may redefine Plan-owned thesis, scope, cells, claims, packet
order, clearance, phase exit, or live status. No other V2 document may redefine
a formula, threshold, recovery class, or owner contract assigned above. Links
are preferred to copied prose.

## Reading order

1. Read [`program-charter.md`](program-charter.md) for the problem and
   falsifiable working hypothesis within Plan scope.
2. Read [`architecture.md`](architecture.md) for the provisional GGML seam
   and provider escalation ladder.
3. Read [`major-milestones.md`](major-milestones.md) for M0 and Plan packets
   A through G.
4. Use the mathematics, safety/evidence, and research documents as the
   corresponding milestone requires.
5. Use the decision record only to understand why a binding contract exists.

## V2 corrections

- Llama 3.1 8B is the conventional foundation cell; Ornith 9B is a separate
  hybrid-state stress cell.
- Requested 10 GiB and 12 GiB tiers are the primary constrained cells. An 8 GiB
  tier is stress-only. The effective live cap can be lower than every requested
  tier.
- A derived representation starts from a pinned BF16/FP16 source or another
  explicitly approved parent. It is never silently requantized from an existing
  lossy Q4 artifact.
- Weight representation is fixed within a static validation cell. Selecting a
  different representation creates a new artifact/provider cell.
- Runtime family/backend, native/WSL/Linux mode, scheduler and cache policy,
  concurrency, and the remaining service-profile fields are immutable cell
  identity rather than optimizer actuators. A material change requires a new
  cell and fresh calibration.
- External runtimes are mechanism references or conditional substrate
  candidates unless the paired-cell direct-comparator predicate passes. They
  remain separate cells even when directly comparable; wrappers and
  orchestration products are not tensor-execution baselines by themselves.
- Decode, prefill, and KV/state execution have separate provider and acceptance
  paths.
- Offline entropy/rate-distortion diagnostics may begin early only as declared
  C0 research. They grant no provider, artifact, runtime, or performance credit
  before the Plan-owned optional-mechanism gate.
- Joint optimization opens only after at least two independent mechanisms pass
  their own gates.

## Markdown mathematics

V2 uses `$...$` for inline mathematics and blank-line-separated
`$$...$$` blocks for display mathematics. Legacy backslash-parenthesis
and backslash-bracket delimiters are prohibited, as are formulas embedded in
tables.
Every display block must render in VS Code Markdown preview and GitHub-compatible
Markdown before the archive migration is accepted.

## Migration

The superseded packet is preserved at
[`../archive/adaptive-runtime-v1-2026-07-11/`](../archive/adaptive-runtime-v1-2026-07-11/).
Archived status statements and links describe the repository when V1 was
written. They are not active authority.

GitHub Project #2 mirrors the M0-M7 sequence and the live issue/PR state in
`Plan.md`. Packet branches created before M0 were required to rebase after the
migration and port any still-applicable V1 edits into the sole V2 owner
document. Current and future branches must not recreate the former V1 active
directory.
