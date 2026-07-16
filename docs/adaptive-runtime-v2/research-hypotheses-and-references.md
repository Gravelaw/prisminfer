# Research Hypotheses, Novelty Boundaries, and References

Access window: 2026-07-10 through 2026-07-16. Paper results establish precedent
on the authors' cells, not expected PrismInfer results. Every runtime dependency,
artifact, recipe, and evaluation input must be pinned separately.

## Entropy is three different questions

Token cross-entropy measures predictive modelling and is reported in nats or
bits per token. Tensor-symbol entropy describes a declared discrete stream such
as quantized codes, scale deltas, indexes, or residual symbols. Operational
effective rate counts every byte that must be stored, moved, retained, decoded,
or indexed. None can be substituted for another.

Entropy is a diagnostic and coding lower bound. It does not imply that a
variable-length stream is random-access, SIMD/SIMT-friendly, or faster than an
already packed fixed-rate tensor.

## Rotation does not remove information

An orthogonal transform preserves norms, inner products, volume, and joint
differential entropy. Its value is to redistribute coordinate outliers or
coherence so a constrained scalar/lattice quantizer may achieve lower
distortion. The transform must be legally absorbed or inverted across every
linear map, residual branch, nonlinearity, RoPE boundary, attention operation,
and mutable state transition. Arbitrary rotations are not runtime switches.

The pilot compares identity, random legal Hadamard-style transforms, and a
learned/legal transform only at matched effective rate. Because rotation may
flatten the activation-outlier signal used by residual routing, representation
and routing require a factorial ablation.

## Progressive representation hypothesis

The promising lane is not a generic additive residual equation. It is a
versioned decoder with an executable fixed-rate base and only declared prefixes:

$$
\widehat W^{(m)}
=
\operatorname{Decode}(B_0, R_1, \ldots, R_m),
\qquad
m \in \{0,\ldots,M\}.
$$

Additivity is tested only for a format that guarantees it. Every prefix has an
independent byte, quality, random-access, workspace, and kernel record. The
runtime cannot fetch `R_3` without satisfying the declared dependency
prefix, and it cannot reconstruct a full higher-precision model persistently.

## Entropy placement hypothesis

The hot base remains fixed-rate and directly executable. Entropy coding is
considered for cold residual/index streams only when:

- symbol skew survives indexes, padding, chunk framing, checksums, and random
  access;
- maximum expansion and decode workspace are bounded;
- CPU and GPU reference decoders are exact;
- exposed fetch plus decode beats the uncompressed path end to end; and
- decode and prefill pass separately.

This responds to the systems tension between HyperQuant's fused Rice-coded path
and ZipServ's fixed GPU-friendly encoding: the correct format depends on the
consuming kernel and phase, not Shannon entropy alone.

## Current novelty boundary

The following are not PrismInfer novelty claims:

- rotation plus scalar, lattice, or vector quantization;
- rotation plus entropy coding;
- progressive/nested precision or base-plus-residual weights;
- compressed-aware weight GEMV/GEMM;
- KV quantization, retention, or progressive transfer;
- CPU/GPU/NVMe placement optimization;
- speculative decoding or speculative offload;
- conditional depth or activation-aware sparsity;
- autotuning and measurement-driven kernel selection.

The publishable question is narrower: whether the exact composition in the
charter, under fail-closed consumer-Windows admission and actual-path evidence,
produces a new measured system result or a useful negative bound.

## Optional mechanisms are preserved, not excluded

| Mechanism | Supporting precedent | Why it remains gated |
|---|---|---|
| KV quantization and placement | [KIVI](https://arxiv.org/abs/2402.02750), [KVQuant](https://arxiv.org/abs/2401.18079), [KVPR](https://aclanthology.org/2025.findings-acl.997/), and [Lynx](https://arxiv.org/abs/2607.01831) | Mutable state, attention consumption, retrieval quality, response length, and PCIe overlap differ from immutable weights |
| Progressive base plus residual weights | [MatGPTQ](https://arxiv.org/abs/2602.03537), [Any-Precision](https://arxiv.org/abs/2402.10517), [BitStack](https://arxiv.org/abs/2410.23918), and [DecDEC](https://www.usenix.org/conference/osdi25/presentation/park-yeonhong) | Requires a new source-derived artifact, prefix decoder, random access, direct kernels, and matched-rate quality |
| Activation-transfer compression | [FourierCompress](https://arxiv.org/abs/2510.16418), [SharQ](https://arxiv.org/abs/2606.26587), and [HeteGen](https://arxiv.org/abs/2403.01164) | Collaborative/network and accelerator results do not prove a local PCIe win; online encode/decode and consumer cost must pass T-046 |
| Structured-compute oracle and router | [DejaVu](https://arxiv.org/abs/2310.17157), [PowerInfer](https://arxiv.org/abs/2312.12456), [Mixture-of-Depths](https://arxiv.org/abs/2404.02258), and [LayerSkip](https://arxiv.org/abs/2404.16710) | Much of the prior art relies on model adaptation or different sparsity assumptions; a training-free oracle must pass before router work |
| Committed-output-aware speculative offload | [Speculative Decoding](https://proceedings.mlr.press/v202/leviathan23a.html), [SpecOffload](https://arxiv.org/abs/2505.10259), [Dovetail](https://aclanthology.org/2025.emnlp-main.879/), and [Lynx](https://arxiv.org/abs/2607.01831) | Draft acceptance is not the objective; target verification, extra memory, bytes, rollback, and committed output are charged |
| Custom kernel dispatch and bounded staging | [HyperQuant](https://arxiv.org/abs/2606.23406), [ZipServ](https://arxiv.org/abs/2603.17435), [Marlin](https://github.com/IST-DASLab/marlin), [APEX](https://arxiv.org/abs/2506.03296), and [ATSInfer](https://arxiv.org/abs/2607.10183) | Decode and prefill differ; a microkernel win must survive source duplication, workspace, transfer, fallback, and full-model replay |
| Joint optimization after independent passes | [FlexGen](https://arxiv.org/abs/2303.06865) and [ATSInfer](https://arxiv.org/abs/2607.10183) | Coupling can hide a failed component and overfit one machine; T-049 opens only after two independent passes |

The gates are intended to prevent implementation cost from outrunning evidence.
They do not reject the mechanisms.

## Runtime comparators

This comparison reflects the public repositories reviewed on 2026-07-16. It is
architectural evidence, not a transfer of either project's benchmark results.

| Runtime | What its current scope demonstrates | PrismInfer consequence |
|---|---|---|
| [Colibri](https://github.com/JustVugg/colibri) | A narrow, self-contained C runtime can exploit architecture-specific properties of GLM-5.2 MoE, including routed-expert streaming across disk, RAM, and VRAM | Reuse its expert-streaming and exact-validation lessons as comparators; it does not establish a general substrate or a dense Llama/Ornith result |
| [ds4 / DwarfStar](https://github.com/antirez/ds4) | A deliberately one-model runtime can combine accelerator backends, SSD-routed-expert streaming, and distributed layer splitting for DeepSeek V4 Flash/PRO | It defines a credible clean-sheet pattern when one architecture and high-memory target justify ownership; its 96/128 GB-class target does not answer PrismInfer's 10/12 GiB cells |
| [llama.cpp](https://github.com/ggml-org/llama.cpp) | A broad GGUF runtime supplies mature model loading, quantized operators, heterogeneous backends, and public controls | It remains the provisional substrate and strongest control, subject to the Packet D seam-proof spike against the pinned commit |
| PrismInfer V2 | A cap-aware control, admission, representation, provider, and evidence layer for constrained consumer devices | Own the policy and experimental mechanisms first; do not duplicate the loader, graph runtime, or backend fleet until the explicit reopen criteria are met |

Colibri and ds4 therefore do not make the PrismInfer direction incorrect. They
show that clean-sheet ownership can pay when it is intentionally architecture
specific. PrismInfer should reopen that choice only after the pinned GGML seam
fails, a second mature substrate is evaluated, and the architecture document's
cost and reproducibility criteria are satisfied.

## Representation and execution sources

| Source | Established precedent | PrismInfer boundary |
|---|---|---|
| [HyperQuant](https://arxiv.org/abs/2606.23406) | Randomized Hadamard transforms, lattice quantization, Rice coding, weights/KV, and phase-specific fused execution | Direct composite baseline; no entropy/rotation novelty claim |
| [ZipServ](https://arxiv.org/abs/2603.17435) | GPU-friendly lossless encoding and fused decompression/compute | Variable-length coding must beat a fixed-format control |
| [QuaRot](https://arxiv.org/abs/2404.00456) | Training-free rotations for low-bit transformer inference | Legality and matched-rate baseline |
| [SpinQuant](https://arxiv.org/abs/2405.16406) | Learned rotations for quantization | Learned-transform baseline; training cost recorded |
| [QuIP#](https://proceedings.mlr.press/v235/tseng24a.html) | Incoherence processing and lattice quantization | Derived-format and rate-distortion precedent |
| [HIGGS](https://aclanthology.org/2025.naacl-long.543/) | Hadamard-based Gaussian lattice quantization | Lattice/rotation baseline |
| [ReSpinQuant](https://arxiv.org/abs/2604.11080) | Rotation refinements for quantized inference | Emerging transform comparator |
| [OptRot](https://arxiv.org/abs/2512.24124) | Optimization of rotations for quantization | Emerging learned/legal transform comparator |
| [MatGPTQ](https://arxiv.org/abs/2602.03537) | Sliceable multi-precision representation with execution support | Direct progressive representation baseline |
| [Any-Precision LLM](https://arxiv.org/abs/2402.10517) | Nested precision levels from one representation | Progressive-format precedent |
| [BitStack](https://arxiv.org/abs/2410.23918) | Scalable residual/bit-plane-like model representation | Base-plus-residual precedent |
| [DecDEC](https://www.usenix.org/conference/osdi25/presentation/park-yeonhong) | CPU-resident residual correction selected by activation outliers | Residual routing baseline and rotation interaction |
| [AQLM](https://arxiv.org/abs/2401.06118) | Additive vector quantization for LLM weights | Vector-quantization baseline |
| [Marlin](https://github.com/IST-DASLab/marlin) | Optimized autoregressive FP16 x INT4 matmul | Compatible-kernel baseline only |
| [QServe](https://arxiv.org/abs/2405.04532) | W4A8KV4 algorithm/system co-design | Nominal bits do not imply serving speed |

## KV, state, and speculation sources

| Source | Established precedent | PrismInfer boundary |
|---|---|---|
| [KIVI](https://arxiv.org/abs/2402.02750) | Asymmetric low-bit KV with residual window | Independent KV candidate |
| [KVQuant](https://arxiv.org/abs/2401.18079) | Pre-RoPE keys, non-uniform quantization, and outliers | Separate KV candidate |
| [TurboQuant](https://arxiv.org/abs/2504.19874) | Fast vector/KV quantization and inner-product preservation | Offline then exact-cell candidate |
| [QJL](https://arxiv.org/abs/2406.03482) | Quantized JL transforms for KV | Sketch/error baseline |
| [Lynx](https://arxiv.org/abs/2607.01831) | Anchor/residual KV transfer with speculation and verification | Emerging direct comparator; replicate locally |
| [Rethinking KV Cache Compression](https://proceedings.mlsys.org/paper_files/paper/2025/hash/26289c647c6828e862e271ca3c490486-Abstract-Conference.html) | Byte savings can lose throughput and change output length | Requires full response-length and retrieval evaluation |
| [Speculative Decoding](https://proceedings.mlr.press/v202/leviathan23a.html) | Exact target-distributed speculative sampling | Count committed target-distributed output |
| [SpecOffload](https://arxiv.org/abs/2505.10259) | Speculation combined with target offload | Direct transfer-amortization baseline |
| [Dovetail](https://aclanthology.org/2025.emnlp-main.879/) | Heterogeneous CPU/GPU speculative decoding on resource-constrained systems | Direct comparator; count all target work and extra state |

## Placement, runtime, and measurement sources

| Source | Established precedent | PrismInfer boundary |
|---|---|---|
| [llama.cpp](https://github.com/ggml-org/llama.cpp) | GGUF inference and heterogeneous CPU/GPU backends | Pin exact commit; strongest public-control baseline |
| [Pinned llama.cpp/GGML commit](https://github.com/ggml-org/llama.cpp/commit/ef2d770117db45b05aa7ecd1b0acca36370c5470) | Repository dependency identity | Features on current master do not prove availability in the pin |
| [Pinned GGML backend header](https://github.com/ggml-org/llama.cpp/blob/ef2d770117db45b05aa7ecd1b0acca36370c5470/ggml/include/ggml-backend.h) | Buffer and scheduler surface to test | Seam-proof evidence is still required |
| [FlexGen](https://arxiv.org/abs/2303.06865) | Optimized GPU/CPU/disk placement | Aggregate/batched throughput is not interactive latency |
| [APEX](https://arxiv.org/abs/2506.03296) | Profiled CPU/GPU parallel execution | Hardware/OS-specific replication required |
| [ATSInfer](https://arxiv.org/abs/2607.10183) | Tensor-granular placement and asynchronous CPU/GPU coordination on consumer devices | Very recent direct scheduling comparator; no result transfers without exact-cell replication |
| [HeteGen](https://arxiv.org/abs/2403.01164) | Heterogeneous parallel CPU/GPU inference | Supports measured co-execution; workload and hardware differ |
| [KVPR](https://aclanthology.org/2025.findings-acl.997/) | Partial KV recomputation overlapped with PCIe transfer | Recompute-versus-transfer resource-DAG precedent |
| [PowerInfer](https://arxiv.org/abs/2312.12456) | Activation-locality-aware consumer inference | Activation assumptions require local evidence |
| [LLM in a Flash](https://arxiv.org/abs/2312.11514) | Storage-aware inference and contiguous loading | Addressability is not latency evidence |
| [PagedAttention](https://arxiv.org/abs/2309.06180) | Paged KV management | Does not solve weight residency |
| [Ansor](https://www.usenix.org/conference/osdi20/presentation/zheng) | Measurement-driven tensor program search | General calibration precedent |

## Activation and structured-compute sources

| Source | Established precedent | PrismInfer boundary |
|---|---|---|
| [FourierCompress](https://arxiv.org/abs/2510.16418) | Layer-aware spectral activation compression for collaborative LLM inference | Supports the activation-transfer hypothesis; network split and smaller models differ from local PCIe |
| [SharQ](https://arxiv.org/abs/2606.26587) | Joint activation sparsity and FP4 quantization | Emerging activation-format comparator; requires supported consumer kernels |
| [Harmonia](https://arxiv.org/abs/2602.04595) | BFP activation/KV representation with algorithm-hardware co-design | Custom-accelerator precedent, not a deployable CUDA result |
| [DejaVu](https://arxiv.org/abs/2310.17157) | Contextual sparsity prediction for inference | Oracle/router precedent; exact model and kernel assumptions must be retested |
| [Mixture-of-Depths](https://arxiv.org/abs/2404.02258) | Token-level routing under a compute budget | Model-training/co-design precedent, not a runtime-only switch |
| [LayerSkip](https://arxiv.org/abs/2404.16710) | Layer dropout and early-exit/self-speculative decoding | Requires model adaptation; supports a separate research lane |

## Information and quality sources

- [Shannon, A Mathematical Theory of Communication](https://doi.org/10.1002/j.1538-7305.1948.tb01338.x)
  defines entropy and coding limits.
- [Shannon, Coding Theorems for a Discrete Source With a Fidelity
  Criterion](https://ieeexplore.ieee.org/document/5311476) establishes the
  rate-distortion framing.
- [Language Modeling Is Compression](https://openreview.net/forum?id=jznbgiynus)
  relates predictive modelling and compression; it does not make tensor
  entropy a runtime speed predictor.

## Source-use rules

- Prefer papers, official documentation, and official repositories.
- Pin commits and revisions in manifests; current master is a novelty check
  only.
- Never import a paper's speedup, memory figure, or quality result.
- Compare exact artifacts, contexts, caps, phases, workloads, and quality sets.
- Record failed, slower, capacity-only, and not-admitted outcomes.
- A microkernel result is not a full-model result.
- A representation requires a canonical decoder, effective-rate accounting,
  quality evidence, bounded workspace, and actual-path acknowledgement.
