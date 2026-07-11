# Session Thesis and Evidence Map

Date: 2026-07-10
Status: council input; not an implementation or performance claim

This document preserves the reasoning developed in the PrismInfer discussion
that preceded the adaptive-runtime council. It is a structured reconstruction,
not a verbatim transcript. Its purpose is to prevent the council from silently
dropping ideas, overstating hypotheses as facts, or repeating work already
available in llama.cpp and the research literature.

The council must read this document together with:

- `docs/research-roadmap-constrained-llm-inference.md`,
- `docs/phase5-compute-kernel-research-plan.md`,
- `docs/phase6-compression-architecture.md`,
- `docs/phase6-implementation-plan.md`, and
- `docs/adaptive-runtime/council-record.md`, which preserves the binding
  specialist findings and adversarial disposition.

## Session Goal

The original goal was to understand whether a 30B to 90B model can be served
usefully on a machine with at most 16 GiB of VRAM despite weight, KV-cache,
workspace, host-transfer, and latency constraints. The discussion converged on
a narrower and falsifiable thesis:

> PrismInfer should be a calibrated heterogeneous execution-planning layer over
> llama.cpp, GGML, and GGUF. It should jointly choose representation, kernel,
> placement, transfer, KV, and optional speculative or structured-compute
> policies under explicit memory, latency, and quality envelopes.

This does not claim that a 90B model will be interactive on the current
hardware. It defines the system needed to measure when such a profile is useful,
slow/offline, or infeasible.

## Questions Raised During the Session

1. Does PrismInfer require an optimizer model, and what should it optimize?
2. Must calibration capture telemetry from the device and from another runtime?
3. After calibration, does PrismInfer use its own execution runtime?
4. Can calibration choose among multiple matrix-multiplication algorithms or
   kernels?
5. Does AlphaTensor-style matrix-multiplication discovery change the thesis?
6. Which other runtime layers can PrismInfer tune besides matrix
   multiplication?
7. Can weights, layers, KV blocks, or activations be compressed and
   decompressed in real time according to the request and hardware?
8. Can the prompt or task activate only selected layers or neurons in an
   already-trained dense model?
9. Can speculative decoding amortize streamed target-model weights over
   multiple accepted tokens?
10. Should the runtime be written in C++, CUDA C++, Mojo, or a mixture?
11. Which high-capability 9B model should be the first representative cell?

## Council Interpretation of the Session

| Session idea | Evidence status | Binding interpretation for planning |
|---|---|---|
| Use an optimizer model | Plausible, but a learned neural optimizer is not required for the MVP. | Start with measured candidate enumeration, conservative cost models, constrained search, and Pareto selection. Learned ranking or safe contextual bandits are later options only if simple models fail. |
| Optimize placement, compression, and CPU/GPU partition | Supported as a systems formulation. | Optimize a resource-DAG makespan or committed target-distributed output tokens per second while enforcing VRAM, RAM, quality, and tail-latency constraints. Include switching and transfer costs. |
| Learn the device through calibration | Supported and required. | Fingerprint the hardware/software cell; measure CPU, GPU, memory, PCIe, allocator, kernel, llama.cpp, and workload-phase behavior; retain raw evidence and prediction error. |
| Use PrismInfer's own runtime after calibration | Partly supported. | PrismInfer owns plan selection, policy, evidence, and guarded dispatch. llama.cpp/GGML remains the default loader and executor. PrismInfer may progressively own selected operators or scheduling paths only after a measured integration gate. |
| Select among three matmul algorithms | Supported as autotuning. | Maintain an exact-shape kernel registry and benchmark GGML, vendor-library, template-library, and approved custom candidates. Cache the winner by validation-cell key. Do not choose from headline benchmark results. |
| Invent a new matrix-multiplication algorithm | Valid research, not an MVP dependency. | AlphaTensor establishes that algorithm discovery can find hardware-specific improvements. PrismInfer should expose an experimental candidate lane, but its main novelty is joint scheduling and representation selection, not asymptotic matmul research. |
| Compress/decompress only needed layers | Misleading for an ordinary dense transformer. | Every dense layer normally contributes. Use offline-created random-access quantized or base-plus-residual artifacts and bounded fused reconstruction. Do not compress the immutable source GGUF in place. |
| Prompt-dependent layer/neuron activation | Research-backed but conditional. | Prompt/task clustering may warm-start a policy. Per-layer hidden-state routing must make structured block decisions and measure router overhead. After approximate state/output commits, a dense audit is diagnostic unless pre-commit verification or rollback exists. Full layer skipping normally requires training or adaptation. |
| A clusterer can make a dense model act like MoE | Unproven as stated. | Contextual sparsity exists in some models, but irregular sparsity may not yield wall-clock speedup. Structured blocks, coverage, quality, and hardware efficiency must all pass. |
| Compression automatically speeds prefill | Not generally true. | Smaller encoded weights reduce storage and transfer bytes, but prefill can become compute-bound and reconstruction can dominate. Measure TTFT and prompt tokens/s, not bytes alone. |
| Dynamic KV/state precision and residency | Strong research support for full-attention KV; architecture-specific elsewhere. | Treat full-attention KV and hybrid recurrent/convolution state as separate governed objects. Start with exact architecture-state accounting and fixed policies before online adaptation. |
| Speculation amortizes transfers | Supported as a research lane. | Optimize observed external bytes per committed target-distributed output token and include draft cost, accepted draft length, the mandatory target correction/extra token, verification, rollback, and state effects. |
| C++ versus Mojo | Current ecosystem favors C++20/CUDA. | Use C++20 for the runtime and adapters, CUDA C++ or vendor libraries for NVIDIA kernels, and Python for offline evaluation. Keep Mojo as a separately benchmarked experimental kernel lane. |
| Use Ornith-1.0-9B | Plausible candidate, not yet a pinned cell. | Make Ornith a capability/stress candidate only after llama.cpp/GGUF compatibility, license, conversion, hashes, tokenizer, and quality fixtures are certified. Retain at least one architecture/control model. |

## Proposed Optimization Problem

For request class \(r\), execution phase \(s\), and a candidate plan \(\pi\),
the first planner should minimize a measured risk-adjusted cost:

\[
J(\pi \mid r,s,h,m) =
  T_{\mathrm{critical}}(\pi)
  + \lambda_b B_{\mathrm{critical}}(\pi)
  + \lambda_e E(\pi)
  + \lambda_q Q_{\mathrm{risk}}(\pi)
  + \lambda_v V_{\mathrm{prediction}}(\pi)
  + \lambda_x C_{\mathrm{switch}}(\pi)
\]

where \(h\) is the hardware/software fingerprint and \(m\) is the exact model
artifact. For speculative profiles, a primary reported metric is:

\[
\mathrm{target\ weight\ bytes\ per\ committed\ output\ token} =
\frac{B_{\mathrm{target\ weights}}}
     {\mathbb{E}[N_{\mathrm{committed\ output}}]}.
\]

The hard constraints are at least:

\[
\begin{aligned}
\mathrm{peak\_vram}(\pi) &\leq C_{\mathrm{vram}} \leq 16\ \mathrm{GiB},\\
\mathrm{peak\_host}(\pi) &\leq C_{\mathrm{host}},\\
\Delta \mathrm{quality}(\pi) &\leq \epsilon_q,\\
\mathrm{TTFT}_{p95}(\pi) &\leq L_{\mathrm{prefill}},\\
\mathrm{ITL}_{p95}(\pi) &\leq L_{\mathrm{decode}},\\
\mathrm{unknown\_memory\_bytes}(\pi) &= 0
\end{aligned}
\]

for a promoted cap-certified result. The optimizer must reject a plan when a
hard constraint cannot be evidenced; it must not turn missing evidence into a
soft penalty.

## Hardware Hierarchy Discussed

The session proposed treating the machine as an actively managed hierarchy:

```text
GPU registers and shared memory
  -> GPU L2 and VRAM
  -> pinned host staging memory
  -> ordinary host RAM and OS page cache
  -> NVMe-backed model pages or derived artifacts
```

The analogy to a CPU cache is useful, but incomplete. A transformer has a mostly
known execution graph, while transfer, quantization, kernel launch, and
approximation costs differ across objects. Weight tensors, KV pages, workspaces,
and activation blocks therefore require different policies and evidence.

## Runtime Layers the Council Must Consider

The discussion expanded the optimizer beyond matrix multiplication. The
architecture review must cover all of these decision surfaces:

| Layer | Candidate decisions |
|---|---|
| Artifact | GGUF quant type, optional derived packed form, immutable hashes, random-access base/residual representation. |
| Model loading | mmap/mlock/direct-IO policy, page warming, retained host copy, pinned staging limits. |
| Placement | per-tensor buffer type, contiguous layer regions, CPU/GPU split, KV residency, workspace reservation. |
| Transfer | tile size, double buffering, prefetch distance, streams/events, eviction, cold versus warm cache. |
| Kernel | GGML path, cuBLAS/cuBLASLt, CUTLASS, approved custom kernel, exact shape/layout/workspace key. |
| Attention and KV | KV type, page/block size, compression, importance/age, promotion/demotion, prefix reuse. |
| Execution phase | distinct load/context/request/operator lifecycle controls with actuator-specific local, compatible-boundary, or restart/reject recovery. |
| Conditional compute | structured MLP/head blocks, hidden-state router, confidence, pre-commit verification or explicitly lossy behavior, optional adaptation. |
| Speculation | draft model/device, draft length, acceptance threshold, target verification batch, rollback. |
| Evidence | validation-cell identity, plan hash, raw telemetry, quality fixture, profiler artifact, classification. |

## Matrix-Multiplication Discussion

The shared video was [How AI Discovered a Faster Matrix Multiplication
Algorithm](https://youtu.be/fDAPJ7rvcUw). Its relevant research source is the
AlphaTensor work. The session conclusion was:

- algorithm discovery proves that "matrix multiplication" is not one immutable
  implementation;
- hardware-aware algorithms can outperform generic choices for selected shapes;
- LLM prefill and decode exercise very different shapes, layouts, precisions,
  and arithmetic intensities;
- therefore PrismInfer should calibrate a kernel portfolio;
- however, a lower arithmetic count does not guarantee a faster quantized LLM
  path after packing, dequantization, memory traffic, numerical error, launch,
  and integration costs.

The kernel search key must include at least operation, model tensor identity,
quantization format, layout, \(M,N,K\), batch/context regime, alignment, GPU
architecture, CUDA/driver versions, workspace cap, and prefill/decode phase.

## Compression and Conditional-Compute Discussion

### What is credible first

- Keep GGUF weights in their quantized representation.
- Fuse or tile dequantization so a full high-precision matrix is never resident.
- Quantize or compress KV blocks under task-level quality gates.
- Create derived random-access artifacts offline and bind them to the immutable
  source model hash.
- Explore base-plus-residual precision where a low-bit base is always available
  and correction streams are fetched or reconstructed selectively.
- Route structured blocks, not individual scalar neurons, when hardware speed is
  the objective.
- Use hidden-state signals for per-layer decisions and prompt clusters only to
  select an initial profile or prefetch set.

### What remains a hypothesis

- That a training-free router for an arbitrary modern dense 9B model will skip
  enough structured compute to beat routing, gather, and fallback overhead.
- That base-plus-residual weights can improve end-to-end prefill or decode on
  this exact GPU/CPU/PCIe cell.
- That online entropy coding of already-quantized weights is profitable.
- That full layer skipping can preserve quality without fine-tuning.
- That a 30B to 90B target can reach interactive latency on the current host.

## Runtime Boundary Reached in the Session

The agreed integration ladder is:

1. **Observe and govern** a pinned llama.cpp executable and retain exact
   baselines.
2. **Control supported llama.cpp/GGML knobs** such as buffer placement, GPU
   layers, KV types, fit margins, and speculative parameters.
3. **Link through a stable adapter** to receive operator, allocation, and timing
   events without parsing logs.
4. **Add guarded dispatch hooks** for exact operators or buffer types whose
   candidate implementations pass differential and end-to-end tests.
5. **Own a specialized execution path only where evidence justifies it.**

PrismInfer is therefore not assumed to become a clean-sheet runtime immediately
after calibration. Calibration produces a versioned execution-plan bundle; the
executor initially replays that bundle through llama.cpp/GGML and only later
contains evidence-approved specialized paths.

## 9B Representative-Cell Discussion

The session proposed Ornith-1.0-9B because it is a high-capability 9B-class
model. Its Qwen3.5-family stack has dense parameter routing but is a hybrid of
Gated DeltaNet/linear-attention and full-attention layers, with multimodal
components in the source configuration. That makes it useful as a capability
and architecture-state stress case, but not as the sole generic full-attention
foundation. Benchmark scores alone are not a systems-selection criterion.

The council must define at least:

- a conventional full-attention 8B/9B foundation cell with a pinned
  llama.cpp/GGUF path;
- a capability/hybrid stress cell: Ornith-1.0-9B if its main artifact,
  modality, optional mmproj/MTP, operator coverage, and architecture-state
  accounting pass;
- Qwen3.5-9B as a lineage comparator rather than an independent architecture
  control;
- a smaller smoke model for rapid correctness and fault-injection tests.

No third-party GGUF should become the canonical evidence artifact without a
pinned source revision, conversion/quantization recipe, tokenizer and chat
template, hashes, and task-quality comparison. Third-party quantizations may be
used for non-promoted smoke tests.

## Papers and Systems Explicitly Connected to the Session

The canonical annotated citations belong in `references.md`; this list records
why each family entered the conversation.

| Theme | Sources discussed | Relevance to the thesis |
|---|---|---|
| Heterogeneous offload | FlexGen, HeteGen, PowerInfer, PowerInfer-2, APEX, LLM in a Flash | Placement, overlap, host/storage limits, and CPU/GPU execution. |
| Speculative offload | SpecOffload and SubSpec | Amortizing target-weight transfer over committed target-distributed output tokens; acceptance is a diagnostic. |
| KV management | PagedAttention/vLLM, vAttention, H2O, SnapKV, PyramidKV, MiniCache | Paging, retention, eviction, and memory-aware attention. |
| KV quantization | KIVI, KVQuant, QServe, QJL, PolarQuant, TurboQuant, Lynx | Effective-bit accounting, asymmetric precision, vector preservation, and progressive streams. |
| Weight quantization | AWQ, AQLM, QuIP#, QQQ, Marlin, T-MAC | Weight representation, fused kernels, CPU low-bit execution, and prefill/decode differences. |
| Conditional compute | DejaVu, PowerInfer, Mixture-of-Depths, LayerSkip, Wanda | Contextual sparsity, activation prediction, layer skipping, structured pruning, and training requirements. |
| Dynamic precision | DP-LLM | Runtime layer-wise precision selection and the need for selectors/adaptation. |
| Kernel selection | cuBLASLt, CUTLASS, Triton, FlashAttention, FlashInfer | Exact-shape heuristics, autotuning, IO-aware kernels, and attention baselines. |
| Algorithm discovery | AlphaTensor and AlphaEvolve | Long-term hardware-aware algorithm search; not an MVP dependency. |
| Mathematical framing | rate-distortion theory, online optimization, dynamic programming, constrained search, bandits/control | Formal objectives, hard constraints, uncertainty, and safe adaptation. |

## Language Decision from the Session

The current decision is:

- C++20 for the production control plane, adapter, planner, and executor;
- CUDA C++ and vendor/template libraries for NVIDIA hot paths;
- Python for offline analysis, fixture generation, plotting, and model-quality
  evaluation;
- Mojo only in an isolated experiment lane until it has native Windows support,
  stable application-level systems facilities, and an integration story that
  beats the C++/CUDA baseline on the same cell.

This is an interoperability and evidence decision, not a claim that C++ is
intrinsically the best possible language.

## Required Council Outputs

The council must convert the discussion into:

1. an architecture with explicit PrismInfer, llama.cpp/GGML, and hardware
   ownership boundaries;
2. an in-scope, deferred, and out-of-scope decision record;
3. a calibration and plan-replay workflow;
4. a formal optimizer and uncertainty policy;
5. an execution and testing plan with falsifiable gates;
6. an annotated primary-source reference index;
7. a GitHub roadmap whose items preserve dependencies and claim boundaries;
8. an adversarial review that attempts to disprove novelty, feasibility,
   safety, and benchmark validity.

## Session-Level Non-Negotiable Constraints

- Maximum declared GPU cap remains 16 GiB.
- Missing or contradictory memory evidence fails closed.
- The source GGUF remains immutable.
- No full-model high-precision materialization may be hidden by a compression
  claim.
- All comparisons are exact validation-cell comparisons.
- A kernel win must be end-to-end, not only isolated kernel time.
- Prompt-conditioned approximation must use pre-commit verification,
  checkpointed rollback, or an explicitly lossy classification; a later dense
  audit cannot undo committed output/state.
- A 90B result remains simulated, measured-offload, quality-gated, validated,
  or rejected according to retained evidence; it is not assumed interactive.
- Development cost is not an optimization variable for this roadmap. Technical
  dependency, evidence quality, correctness, and research value determine
  sequencing.
