# Research Hypotheses, Novelty Boundaries, and References

Access window: 2026-07-10 through 2026-07-29. Paper results establish precedent
on the authors' cells, not expected PrismInfer results. Every runtime dependency,
artifact, recipe, and evaluation input must be pinned separately.

## Evidence level and applicability

This record distinguishes five kinds of evidence:

- peer-reviewed papers establish precedent only for their reported models,
  hardware, software, workloads, and metrics;
- preprints identify emerging comparators but remain unreviewed and require
  stronger local replication before design commitment;
- official runtime documentation and SHA-pinned source establish that a
  capability or dispatch rule exists at a reviewed revision, not that it is
  available in PrismInfer's pinned dependency;
- project README results and community benchmarks are mechanism observations,
  not independent validation; and
- PrismInfer equations, gates, and provider boundaries are design synthesis
  unless a cited source proves the exact statement.

Every numerical result in this document is therefore author-reported unless a
future retained PrismInfer manifest reproduces it. Source review, simulation,
CPU/offline analysis, and another runtime's result grant no model, CUDA,
capacity, quality, performance, packet-entry, or C2 credit.

Source-table status is explicit. `ArXiv manuscript` describes the cited source
format and does not assert that no reviewed version exists; unless a verified
proceedings page is cited, PrismInfer governs it as emerging/preprint evidence.
`Official proceedings` means a venue-hosted reviewed paper, while `official
source` and `official documentation` establish implementation or capability
context only.

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

The continuous statement requires an absolutely continuous source with finite
differential entropy. It does not imply equal discrete entropy after finite
Cartesian scalar quantization: rotation changes the quantizer cells relative to
the source and can change clipping, symbol probabilities, and distortion. In
the regular high-resolution limit with the same shrinking bin-width schedule,
the first-order rate is rotation invariant. An isotropic Gaussian is an
explicit no-gain control.
The normative equations and assumptions are in
[`optimizer-mathematics.md`](optimizer-mathematics.md).

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

| Mechanism | Supporting precedent | Evidence mix | Why it remains gated |
|---|---|---|---|
| KV quantization and placement | [KIVI](https://proceedings.mlr.press/v235/liu24bz.html), [KVQuant](https://proceedings.neurips.cc/paper_files/paper/2024/hash/028fcbcf85435d39a40c4d61b42c99a4-Abstract-Conference.html), [TurboQuant](https://openreview.net/forum?id=tO3ASKZlok), [Strata](https://www.usenix.org/conference/osdi26/presentation/xie-zhiqiang), [DirectKV](https://www.usenix.org/conference/osdi26/presentation/luo), and [Lynx](https://arxiv.org/abs/2607.01831) | Official proceedings/OpenReview plus one arXiv manuscript | Mutable state, attention consumption, retrieval quality, response length, transfer granularity, and PCIe/NVLink-C2C behavior differ from immutable weights |
| Progressive base plus residual weights | [MatGPTQ](https://arxiv.org/abs/2602.03537), [Any-Precision](https://arxiv.org/abs/2402.10517), [BitStack](https://arxiv.org/abs/2410.23918), and [DecDEC](https://www.usenix.org/conference/osdi25/presentation/park-yeonhong) | Official OSDI proceedings plus arXiv manuscripts | Requires a new source-derived artifact, prefix decoder, random access, direct kernels, and matched-rate quality |
| Activation-transfer compression | [Communication Compression for Tensor Parallel LLM Inference](https://arxiv.org/abs/2411.09510), [FourierCompress](https://arxiv.org/abs/2510.16418), [SharQ](https://arxiv.org/abs/2606.26587), and [HeteGen](https://arxiv.org/abs/2403.01164) | ArXiv manuscripts | Inter-accelerator or collaborative results do not prove a local PCIe win; online encode/decode, metadata, synchronization, and consumer cost must pass T-046 |
| Structured-compute oracle and router | [DejaVu](https://arxiv.org/abs/2310.17157), [TEAL](https://github.com/FasterDecoding/TEAL), [ShadowLLM](https://arxiv.org/abs/2406.16635), [Polar Sparsity](https://arxiv.org/abs/2505.14884), and [Kairox](https://www.usenix.org/conference/osdi26/presentation/jiang-yapeng) | Official OSDI proceedings, official source, and arXiv manuscripts | Predictor error, batching, model adaptation, sparse-kernel availability, and transfer policy can erase nominal work savings; a training-free oracle and dense fallback must pass before router work |
| Committed-output-aware speculative offload | [Speculative Decoding](https://proceedings.mlr.press/v202/leviathan23a.html), [SpecOffload](https://arxiv.org/abs/2505.10259), [Dovetail](https://aclanthology.org/2025.emnlp-main.879/), [Block-Level Verification](https://research.google/pubs/block-level-verification-accelerates-speculative-decoding/), and [Lynx](https://arxiv.org/abs/2607.01831) | Official proceedings/research page plus arXiv manuscripts | Draft acceptance is not the objective; target verification, extra memory, prefetched bytes, rejected suffixes, rollback, and committed output are charged |
| Custom kernel dispatch and bounded staging | [ADAngel](https://www.usenix.org/conference/osdi26/presentation/liu-yao), [HyperQuant](https://arxiv.org/abs/2606.23406), [ZipServ](https://arxiv.org/abs/2603.17435), [Marlin](https://github.com/IST-DASLab/marlin), and [ATSInfer](https://arxiv.org/abs/2607.10183) | Official OSDI proceedings, official source, and arXiv manuscripts | Decode and prefill differ; an offline policy map or microkernel win must survive source duplication, workspace, transfer, fallback, and full-model replay |
| Joint optimization after independent passes | [FlexInfer](https://proceedings.mlsys.org/paper_files/paper/2025/hash/698cfaf72a208aef2e78bcac55b74328-Abstract-Conference.html), [Kairox](https://www.usenix.org/conference/osdi26/presentation/jiang-yapeng), [Strata](https://www.usenix.org/conference/osdi26/presentation/xie-zhiqiang), and [ATSInfer](https://arxiv.org/abs/2607.10183) | Official MLSys/OSDI proceedings plus one arXiv manuscript | Coupling can hide a failed component and overfit one machine; two independent passes establish only M6 eligibility, while T-049 requires separate Packet G admission and execution |

The gates are intended to prevent implementation cost from outrunning evidence.
They do not reject the mechanisms.

## Runtime comparators

This comparison reflects official repositories and documentation reviewed
through 2026-07-29. It is
architectural evidence, not a transfer of either project's benchmark results.

For capability and novelty review only, source heads observed on 2026-07-29
were llama.cpp `caa596ab3f0f8768ee326d6e3d5d39782194676c`, vLLM
`a0c092ee72c0dcefbb3b3e74f97ac62d842e5f4b`, SGLang
`e1f2f9d1fa84cd1b8d9020377fdd707b3a485687`, TensorRT-LLM
`f20ea652dd621006148aa918c970ba686cdda407`, Colibri
`2d623816eecbf740b1fc06eb422974bda0af8931`, and ds4
`54b36ed9ba42da31b24f2d1a5feb075c2475dbb1`. These observations do not
change PrismInfer's dependency pin or make current-head features available.

| Runtime/project | Role for V2 | What its current scope demonstrates | PrismInfer consequence |
|---|---|---|---|
| [llama.cpp](https://github.com/ggml-org/llama.cpp) | Within-cell baseline and provisional substrate | Broad GGUF loading, quantized operators, heterogeneous backends, hybrid CPU/GPU execution, and public controls | Pin the exact commit, sweep its supported controls, and require the Packet D seam proof |
| [Colibri](https://github.com/JustVugg/colibri) | Mechanism reference | Architecture-specific GLM-5.2 MoE routed-expert streaming across disk, RAM, and VRAM | Reuse streaming and exact-validation lessons; it is not a dense Llama/Ornith result |
| [ds4 / DwarfStar](https://github.com/antirez/ds4) | Mechanism reference | A one-model runtime can combine accelerator backends, SSD-routed-expert streaming, and distributed layer splitting | It is a credible clean-sheet pattern for a different architecture and memory tier, not a 10/12 GiB control |
| [vLLM](https://docs.vllm.ai/en/latest/getting_started/installation/) | Serving-mechanism reference | Paged attention, prefix caching, KV offload, speculative decoding, compilation, and distributed serving | Inventory algorithms and measurement fields only; any direct runtime comparison requires a separate paired-cell projection and artifact-equivalence receipt |
| [SGLang](https://docs.sglang.io/) | Serving-mechanism reference | RadixAttention, prefix caching, low-latency/high-throughput scheduling, and multi-GPU serving | Use cache and scheduling ideas as references; its serving results are contextual unless the paired-cell comparator gate passes |
| [TensorRT-LLM](https://nvidia.github.io/TensorRT-LLM/reference/support-matrix.html) | NVIDIA/Linux mechanism reference | NVIDIA-specific precision, kernel, KV, batching, and speculative paths; the official support matrix requires Linux | It is not a native-Windows alternative or direct current-cell baseline |
| [Transformers Serve](https://huggingface.co/docs/transformers/main/serve-cli/serving) | Lightweight serving reference | A Transformers-native serving surface that can expose batching and cache controls without a separate model server | Record applicable control semantics, but keep its artifact/runtime identity separate from the pinned GGUF cell |
| [DeepSpeed Inference](https://deepspeed.readthedocs.io/en/stable/inference-init.html) | Distributed-inference mechanism reference | An inference engine with configurable kernel injection and distributed execution around supported model modules | Use its sharding and kernel-injection contracts as references; it does not answer the current native-Windows single-GPU cell |
| [LMDeploy](https://lmdeploy.readthedocs.io/en/stable/) | Serving reference and conditional substrate candidate | TurboMind/PyTorch engines expose persistent batching, blocked KV cache, quantization, prefix caching, and distributed serving | Inventory its controls now; any prototype still requires a #85 trigger and a separate artifact/runtime cell |
| [MLC-LLM](https://github.com/mlc-ai/mlc-llm) | Conditional substrate candidate | Compilation-oriented deployment across multiple device backends and application surfaces | Evaluate only after a retained #85 seam trigger; compiled artifacts require a new equivalence and evidence contract |
| [ExLlamaV3](https://github.com/turboderp-org/exllamav3) | Consumer-GPU mechanism reference and conditional substrate candidate | EXL3 quantization, cache quantization, dynamic batching, speculative decoding, and consumer-GPU tensor/expert parallelism; [ExLlamaV2](https://github.com/turboderp-org/exllamav2) is archived in favor of this line | Its different quantized-artifact semantics and PyTorch/CUDA extension stack require fresh admission and calibration; it is not an inherited GGUF baseline |
| [ONNX Runtime GenAI](https://onnxruntime.ai/docs/genai/) | Conditional Windows substrate candidate | A preview generation API over ONNX Runtime with tokenization, sampling, and KV-cache management | Preview status and different graph/artifact semantics require a separately calibrated prototype, never inherited llama.cpp credit |
| [OpenVINO Model Server](https://docs.openvino.ai/2026/model-server/ovms_docs_genai.html) | Portability/mechanism reference | Continuous batching and paged attention on Intel-oriented CPU/GPU serving paths | Relevant to an admitted Intel portability cell, not the current NVIDIA/WDDM execution cell |
| [MLX-LM](https://github.com/ml-explore/mlx-lm) | Apple portability and mechanism reference | Apple-Silicon generation, quantization, prompt/rotating KV caches, and distributed inference over MLX | Unified-memory results define a different hardware/runtime cell and cannot establish a discrete-GPU constrained-VRAM claim |
| [Text Generation Inference](https://huggingface.co/docs/text-generation-inference/index) | Historical serving reference | An established continuous-batching, paged-attention, and tensor-parallel server now in maintenance mode | Retain architectural lessons but do not select it as a forward implementation dependency |
| [NVIDIA Triton Inference Server](https://docs.nvidia.com/deeplearning/triton-inference-server/user-guide/docs/index.html) | Orchestration layer | Multi-backend model serving, dynamic/sequence batching, ensembles, metrics, and lifecycle management | Attribute tensor-path evidence to its exact backend; Triton itself is not a direct tensor executor or optimizer actuator |
| PrismInfer V2 | Control/evidence layer | Cap-aware admission, exact-cell calibration, guarded planning, optional providers, and retained negative evidence | Own policy and falsification first; do not duplicate the loader, graph runtime, or backend fleet before a reopen decision |

Colibri and ds4 therefore do not make the PrismInfer direction incorrect. They
show that clean-sheet ownership can pay when it is intentionally architecture
specific. PrismInfer should reopen that choice only after the pinned GGML seam
fails, a second mature substrate is evaluated, and the architecture document's
cost and reproducibility criteria are satisfied.

Ollama, LM Studio, LocalAI, Jan, GPT4All, Open WebUI, KoboldCpp,
text-generation-webui, and similar products are distribution, management, or UX
layers for this comparison. Their tensor-path claims inherit the exact
underlying executor and version; the product name is not a separate runtime
baseline.

A paired-cell direct cross-runtime comparison keeps runtime identities separate
but requires identical hardware/host, OS execution mode, canonical model/source,
tokenizer/template, context, prompt/task, cap, concurrency, arrival process,
scheduler/batching/chunking policy, prefix/KV-cache state, streaming/output
policy, power/thermal policy, non-runtime software/provider identity, quality
contract, and measurement protocol, plus a passing derived-artifact and
quantized-tensor equivalence decision. Otherwise the result remains contextual
evidence. Even a passing pair does not share calibration, plans, or
feasible-oracle membership.

## Representation and execution sources

| Source | Evidence status | Established precedent | PrismInfer boundary |
|---|---|---|---|
| [HyperQuant](https://arxiv.org/abs/2606.23406) | ArXiv manuscript | Randomized Hadamard transforms, lattice quantization, Rice coding, weights/KV, and phase-specific fused execution | Direct composite baseline; no entropy/rotation novelty claim |
| [ZipServ](https://arxiv.org/abs/2603.17435) | ArXiv manuscript | GPU-friendly lossless encoding and fused decompression/compute | Variable-length coding must beat a fixed-format control |
| [QuaRot](https://arxiv.org/abs/2404.00456) | ArXiv manuscript | Training-free rotations for low-bit transformer inference | Legality and matched-rate baseline |
| [SpinQuant](https://arxiv.org/abs/2405.16406) | ArXiv manuscript | Learned rotations for quantization | Learned-transform baseline; training cost recorded |
| [QuIP#](https://proceedings.mlr.press/v235/tseng24a.html) | Official ICML 2024 proceedings | Incoherence processing and lattice quantization | Derived-format and rate-distortion precedent |
| [HIGGS](https://aclanthology.org/2025.naacl-long.543/) | Official NAACL 2025 proceedings | Hadamard-based Gaussian lattice quantization | Lattice/rotation baseline |
| [ReSpinQuant](https://arxiv.org/abs/2604.11080) | ArXiv manuscript | Rotation refinements for quantized inference | Emerging transform comparator |
| [OptRot](https://arxiv.org/abs/2512.24124) | ArXiv manuscript | Optimization of rotations for quantization | Emerging learned/legal transform comparator |
| [MatGPTQ](https://arxiv.org/abs/2602.03537) | ArXiv manuscript | Sliceable multi-precision representation with execution support | Direct progressive representation baseline |
| [Any-Precision LLM](https://arxiv.org/abs/2402.10517) | ArXiv manuscript | Nested precision levels from one representation | Progressive-format precedent |
| [BitStack](https://arxiv.org/abs/2410.23918) | ArXiv manuscript | Scalable residual/bit-plane-like model representation | Base-plus-residual precedent |
| [DecDEC](https://www.usenix.org/conference/osdi25/presentation/park-yeonhong) | Official OSDI 2025 proceedings | CPU-resident residual correction selected by activation outliers | Residual routing baseline and rotation interaction |
| [AQLM](https://arxiv.org/abs/2401.06118) | ArXiv manuscript | Additive vector quantization for LLM weights | Vector-quantization baseline |
| [Marlin](https://github.com/IST-DASLab/marlin) | Official source | Optimized autoregressive FP16 x INT4 matmul | Compatible-kernel baseline only |
| [QServe](https://arxiv.org/abs/2405.04532) | ArXiv manuscript | W4A8KV4 algorithm/system co-design | Nominal bits do not imply serving speed |
| [ADAngel](https://www.usenix.org/conference/osdi26/presentation/liu-yao) | Official OSDI 2026 proceedings | Offline mixed-precision GEMM strategy set and lightweight shape/bit-width dispatcher | Supports a measured policy map for #91; published kernel choices and speedups do not transfer to the Windows cell |

## KV, state, and speculation sources

| Source | Evidence status | Established precedent | PrismInfer boundary |
|---|---|---|---|
| [KIVI](https://proceedings.mlr.press/v235/liu24bz.html) | Official ICML 2024 proceedings | Asymmetric low-bit KV with residual window | Independent KV candidate |
| [KVQuant](https://proceedings.neurips.cc/paper_files/paper/2024/hash/028fcbcf85435d39a40c4d61b42c99a4-Abstract-Conference.html) | Official NeurIPS 2024 proceedings | Pre-RoPE keys, non-uniform quantization, and outliers | Separate KV candidate |
| [TurboQuant](https://openreview.net/forum?id=tO3ASKZlok) | Official ICLR 2026 OpenReview | Fast vector/KV quantization and inner-product preservation | Offline then exact-cell candidate |
| [QJL](https://arxiv.org/abs/2406.03482) | ArXiv manuscript | Quantized JL transforms for KV | Sketch/error baseline |
| [Strata](https://www.usenix.org/conference/osdi26/presentation/xie-zhiqiang) | Official OSDI 2026 proceedings | Hierarchical GPU/CPU/SSD context caching, large-transfer layout, and cache-aware scheduling | SGLang implementation and long-context serving precedent; not a native-Windows or batch-1 transfer result |
| [DirectKV](https://www.usenix.org/conference/osdi26/presentation/luo) | Official OSDI 2026 proceedings | Zero-copy CPU-resident KV access with fused attention on NVLink-C2C systems | GH200/GB200 interconnect result; PCIe staging and transfer must be measured separately |
| [JoLT](https://arxiv.org/abs/2607.12550) | ArXiv manuscript | Tucker-factorized KV plus rotated low-bit residual allocation | July 2026 preprint; reconstruction, factor bytes, and deployable kernels remain unproven in the target cell |
| [CompressKV](https://arxiv.org/abs/2606.24467) | ArXiv manuscript | Retrieval-head token retention and layer-wise budget allocation | June 2026 preprint; GQA, calibration, safety, retrieval, and throughput require local tests |
| [Lynx](https://arxiv.org/abs/2607.01831) | ArXiv manuscript | Anchor/residual KV transfer with speculation and verification | Emerging direct comparator; replicate locally |
| [Rethinking KV Cache Compression](https://proceedings.mlsys.org/paper_files/paper/2025/hash/26289c647c6828e862e271ca3c490486-Abstract-Conference.html) | Official MLSys 2025 proceedings | Byte savings can lose throughput and change output length | Requires full response-length and retrieval evaluation |
| [Speculative Decoding](https://proceedings.mlr.press/v202/leviathan23a.html) | Official ICML 2023 proceedings | Exact target-distributed speculative sampling | Count committed target-distributed output |
| [SpecOffload](https://arxiv.org/abs/2505.10259) | ArXiv manuscript | Speculation combined with target offload | Direct transfer-amortization baseline |
| [Dovetail](https://aclanthology.org/2025.emnlp-main.879/) | Official EMNLP 2025 proceedings | Heterogeneous CPU/GPU speculative decoding on resource-constrained systems | Direct comparator; count all target work and extra state |

## Placement, runtime, and measurement sources

| Source | Evidence status | Established precedent | PrismInfer boundary |
|---|---|---|---|
| [llama.cpp](https://github.com/ggml-org/llama.cpp) | Official source | GGUF inference and heterogeneous CPU/GPU backends | Pin exact commit; strongest public-control baseline |
| [Pinned llama.cpp/GGML commit](https://github.com/ggml-org/llama.cpp/commit/ef2d770117db45b05aa7ecd1b0acca36370c5470) | Official source snapshot | Repository dependency identity | Features on current master do not prove availability in the pin |
| [Pinned GGML backend header](https://github.com/ggml-org/llama.cpp/blob/ef2d770117db45b05aa7ecd1b0acca36370c5470/ggml/include/ggml-backend.h) | Official source snapshot | Buffer and scheduler surface to test | Seam-proof evidence is still required |
| [FlexGen](https://arxiv.org/abs/2303.06865) | ArXiv manuscript | Optimized GPU/CPU/disk placement | Aggregate/batched throughput is not interactive latency |
| [FlexInfer](https://proceedings.mlsys.org/paper_files/paper/2025/hash/698cfaf72a208aef2e78bcac55b74328-Abstract-Conference.html) | Official MLSys 2025 proceedings | Separate prefill/decode CPU-GPU policy selection from a hardware performance estimator | Strong planner precedent; its two server cells do not establish a Windows consumer result |
| [APEX](https://arxiv.org/abs/2506.03296) | ArXiv manuscript | Profiled CPU/GPU parallel execution | Hardware/OS-specific replication required |
| [ATSInfer](https://arxiv.org/abs/2607.10183) | ArXiv manuscript | Tensor-granular placement and asynchronous CPU/GPU coordination on consumer devices | Very recent direct scheduling comparator; no result transfers without exact-cell replication |
| [HeteGen](https://arxiv.org/abs/2403.01164) | ArXiv manuscript | Heterogeneous parallel CPU/GPU inference | Supports measured co-execution; workload and hardware differ |
| [KVPR](https://aclanthology.org/2025.findings-acl.997/) | Official ACL Findings 2025 proceedings | Partial KV recomputation overlapped with PCIe transfer | Recompute-versus-transfer resource-DAG precedent |
| [PowerInfer](https://arxiv.org/abs/2312.12456) | ArXiv manuscript | Activation-locality-aware consumer inference | Activation assumptions require local evidence |
| [Kairox](https://www.usenix.org/conference/osdi26/presentation/jiang-yapeng) | Official OSDI 2026 proceedings | Online neuron balancing, activation-predicted prefetch, and temporal activation caching on consumer PCs | Separate adaptive sparse runtime; dense 8B/9B applicability and Windows behavior must be established rather than inferred |
| [Local MoE CPU-GPU Hybrid Design](https://www.usenix.org/conference/osdi26/presentation/wang-wenxin) | Official OSDI 2026 proceedings | Stream-loading prefill, prefill/decode disaggregation, CPU FP8 kernels, and fine-grained local MoE execution | Dual-socket, high-RAM, RTX 5090 MoE cell; not evidence for a dense 10/12 GiB foundation cell |
| [LLM in a Flash](https://arxiv.org/abs/2312.11514) | ArXiv manuscript | Storage-aware inference and contiguous loading | Addressability is not latency evidence |
| [PagedAttention](https://arxiv.org/abs/2309.06180) | ArXiv manuscript | Paged KV management | Does not solve weight residency |
| [Ansor](https://www.usenix.org/conference/osdi20/presentation/zheng) | Official OSDI 2020 proceedings | Measurement-driven tensor program search | General calibration precedent |

## Activation and structured-compute sources

| Source | Evidence status | Established precedent | PrismInfer boundary |
|---|---|---|---|
| [FourierCompress](https://arxiv.org/abs/2510.16418) | ArXiv manuscript | Layer-aware spectral activation compression for collaborative LLM inference | Supports the activation-transfer hypothesis; network split and smaller models differ from local PCIe |
| [Communication Compression for Tensor Parallel LLM Inference](https://arxiv.org/abs/2411.09510) | ArXiv manuscript | Fine-grained activation quantization for tensor-parallel communication | Benefits are link- and phase-dependent; local encode/decode and synchronization remain charged |
| [SharQ](https://arxiv.org/abs/2606.26587) | ArXiv manuscript | Joint activation sparsity and FP4 quantization | Emerging activation-format comparator; requires supported consumer kernels |
| [Harmonia](https://arxiv.org/abs/2602.04595) | ArXiv manuscript | BFP activation/KV representation with algorithm-hardware co-design | Custom-accelerator precedent, not a deployable CUDA result |
| [DejaVu](https://arxiv.org/abs/2310.17157) | ArXiv manuscript | Contextual sparsity prediction for inference | Oracle/router precedent; exact model and kernel assumptions must be retested |
| [TEAL](https://github.com/FasterDecoding/TEAL/tree/fb7373c93ac3594817c9ee64d4e08b47430a1822) | Official source snapshot | Training-free activation sparsity with custom sparse decode kernels | Nominal sparsity is not an end-to-end win without supported fused kernels and exact-cell quality |
| [ShadowLLM](https://arxiv.org/abs/2406.16635) | ArXiv manuscript | Global contextual head/neuron predictor and custom sparse execution | Predictor calibration, model scale, and reported A100 results require local replication |
| [Polar Sparsity](https://arxiv.org/abs/2505.14884) | ArXiv manuscript | Batch-aware separation of MLP and attention sparsity with custom kernels | Batching can erase MLP sparsity; use as an oracle/failure-mode reference |
| [Mixture-of-Depths](https://arxiv.org/abs/2404.02258) | ArXiv manuscript | Token-level routing under a compute budget | Model-training/co-design precedent, not a runtime-only switch |
| [LayerSkip](https://arxiv.org/abs/2404.16710) | ArXiv manuscript | Layer dropout and early-exit/self-speculative decoding | Requires model adaptation; supports a separate research lane |

## Information and quality sources

- [Shannon, A Mathematical Theory of Communication](https://doi.org/10.1002/j.1538-7305.1948.tb01338.x)
  defines entropy and coding limits.
- [Shannon, Coding Theorems for a Discrete Source With a Fidelity
  Criterion](https://ieeexplore.ieee.org/document/5311476) establishes the
  rate-distortion framing.
- [Language Modeling Is Compression](https://openreview.net/forum?id=jznbgiynus)
  relates predictive modelling and compression; it does not make tensor
  entropy a runtime speed predictor.
- [Gray and Neuhoff, Quantization](https://doi.org/10.1109/18.720541)
  supplies the high-resolution scalar/vector quantization and transform-coding
  context used to distinguish continuous entropy from finite quantizer rate.

## Source-use rules

- Prefer papers, official documentation, and official repositories.
- Label peer-reviewed work, preprints, official runtime capabilities, and
  community observations separately.
- Pin commits and revisions in manifests; current master is a novelty check
  only.
- Never import a paper's speedup, memory figure, or quality result.
- Compare exact artifacts, contexts, caps, phases, workloads, and quality sets.
- Record failed, slower, capacity-only, and not-admitted outcomes.
- A microkernel result is not a full-model result.
- A representation requires a canonical decoder, effective-rate accounting,
  quality evidence, bounded workspace, and actual-path acknowledgement.
