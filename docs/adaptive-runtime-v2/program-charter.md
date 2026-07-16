# V2 Research Charter

## Objective

Determine whether exact, safe, interactive LLM inference on constrained
consumer Windows hardware can be improved by a calibrated control/evidence
layer and independently falsified representation or execution providers over
pinned llama.cpp/GGML.

The program must distinguish four outcomes:

1. the exact cell is admitted and improves capacity only;
2. it improves interactive performance at acceptable quality;
3. it is feasible but slower or operationally unsuitable;
4. it is rejected before execution by a truthful resource or safety bound.

All four are valid research outcomes. Merely loading a model is not success, and
an aggregate-throughput result is not an interactive-latency result.

## Binding authority

The root [`Plan.md`](../../Plan.md) owns the binding program thesis, frozen
scope, model/cap cells, claim classes, dependency order, and clearances. This
charter owns only V2 research questions and the falsifiable working hypothesis
inside that scope. Its explanatory lists cannot add a cell, claim, or activity
that the Plan does not admit.

## Research framing within Plan scope

Within the Plan's binding [`Frozen Scope`](../../Plan.md#frozen-scope), the V2
research framing examines:

- immutable hardware, software, model, artifact, and workload identity;
- a CPU-only trusted outer supervisor and a contained native worker;
- pre-context and post-context admission;
- exact requested-versus-actual actuator acknowledgement;
- allocation, workspace, architecture-state, transfer, storage, timing, energy,
  quality, and recovery evidence;
- offline calibration and a conservative session-static plan;
- public llama.cpp controls before any private hook;
- independent provider experiments only after their Plan gates.

Optional hypotheses remain independent:

- phase-specific kernel dispatch and bounded staging;
- weight representation and progressive residual prefixes;
- KV quantization, placement, retention, and architecture-state policies;
- activation-transfer compression;
- committed-output-aware speculative offload;
- structured-compute oracle followed by a guarded router.

Distillation, continued pretraining, learned model adaptation, and arbitrary
architecture changes are separate artifact/training programs. They cannot be
reported as runtime-only optimization.

## Research questions

### Substrate

Can the pinned llama.cpp/GGML revision expose enough lifecycle, tensor,
allocation, workspace, placement, operator, and state truth for a narrow,
default-safe provider? This is a hypothesis requiring the seam-proof evidence
in [`architecture.md`](architecture.md).

### Representation

Does a directly executable fixed-rate base plus bounded residual prefixes reduce
resident or transferred bytes without full persistent reconstruction? Does
entropy coding remain profitable only on cold, skewed residual streams? Does a
legal rotation improve distortion at matched effective rate after its transform,
metadata, and kernel costs?

### Control

Can an offline-calibrated, feasible-set-first, session-static controller select
an acknowledged plan with small regret against a measured feasible oracle while
never underpredicting the promoted memory peak?

### Scale

After the foundation and exact 30B static cells, do independently passing
mechanisms admit or improve exact 30B, 70B, or 90B cells? Scale claims are made
only for the exact artifact, context, cap, workload, and hardware fingerprint
that produced the evidence.

## Narrow working hypothesis

The proposed composition has five independently testable parts:

- H1: a fixed-rate hot base can execute directly in batch-1 decode;
- H2: residual prefixes are tile-addressable and add useful quality per
  effective bit without full-model materialization;
- H3: entropy coding helps only streams whose measured savings exceed index,
  padding, workspace, and decode costs;
- H4: cap-aware static placement and staging expose enough overlap to beat the
  strongest upstream same-cell plan;
- H5: decode, prefill, and KV/state providers can pass independently.

Failure of one hypothesis does not invalidate the others. A joint experiment
cannot hide an independently failed component.

## Research questions by Plan-owned cell

The binding cell and cap definitions are in the Plan's
[`Model and Quantization Cells`](../../Plan.md#model-and-quantization-cells).
The table below records only why each cell matters to the research questions.

| Cell | Role | Boundary |
|---|---|---|
| Tiny deterministic fixture | CPU/CUDA semantics, parser, fault, and seam proof | No model-quality or scale claim |
| Llama 3.1 8B Instruct | First conventional text/GQA foundation | Exact revision, tokenizer, template, source, recipe, GGUF, and quality set must be pinned |
| Ornith 9B | Second hybrid attention/DeltaNet/convolution/MTP stress cell | State accounting and support are separate from Llama; no uniform-KV assumption |
| Exact 30B | First heterogeneous static-placement truth cell | Executes only after Plan admission and the foundation controller audit |
| Exact 70B and 90B | Resource-DAG lower bounds, then conditional execution | Each artifact executes only if refreshed exact admission permits it |

The Plan fixes 10 GiB and 12 GiB as primary requested tiers and 8 GiB as a
stress-diagnostic tier. A nominal-device/reference tier is recorded from
physical and live evidence, never rounded up to a marketing bucket.

## Applying the Plan-owned claim classes

The binding evidence and result labels are the Plan's
[`Evidence and claim classes`](../../Plan.md#evidence-and-claim-classes). The
research questions may produce any of those positive or negative outcomes, but
this charter cannot promote one class into another.

No paper result, microbenchmark, nominal bit width, configured value, or
requested actuator is promoted as an observed PrismInfer result.

## Non-goals

- replacing tokenization, sampling, model graphs, or every kernel before a
  substrate reopen decision;
- treating mmap, pagefile, unified memory, or pinned host memory as free
  capacity;
- claiming that rotation lowers joint entropy;
- reporting compression without metadata, indexes, alignment, residuals,
  codebooks, checksum, workspace, and source duplication;
- choosing a representation dynamically inside a fixed-artifact static plan;
- treating accepted draft tokens as committed target-distributed output;
- combining mechanisms before independent evidence exists.

## Success and falsification

The program succeeds when it produces an honest classification with retained
evidence. A mechanism is falsified for a cell when it violates a hard safety or
quality gate, loses its declared end-to-end comparison, cannot offer bounded
random access, requires persistent full reconstruction, or cannot acknowledge
the actual path. Threshold ownership is in
[`evidence-thresholds-and-security.md`](evidence-thresholds-and-security.md).
