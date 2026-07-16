# Adaptive Runtime References

Access window: 2026-07-10 to 2026-07-11.

This is the council's annotated source index. Paper or project results establish
feasibility on the authors' evaluated models and hardware; they do not predict a
PrismInfer result. Every implementation dependency must additionally be pinned
by commit/version in the run manifest.

## Session and Local Design Inputs

| Source | Use in the council | Boundary |
|---|---|---|
| [`session-thesis-and-evidence-map.md`](session-thesis-and-evidence-map.md) | Structured reconstruction of the session reasoning relevant to the optimizer, telemetry, post-calibration runtime, matmul portfolio, compression, conditional compute, speculation, language, and 9B choice. | Council input, not a verbatim transcript or performance claim. |
| Speculative-inference design input, reconstructed in [`session-thesis-and-evidence-map.md`](session-thesis-and-evidence-map.md) | Introduced the machine-as-memory-hierarchy framing, transfer-amortized speculation, phase-aware scheduling, predictive prefetch, and living KV. | Local note, not a portable or verbatim transcript. The structured reconstruction and primary-source mappings are the canonical council record. |
| [`council-record.md`](council-record.md) | Consolidates the independent GPU, CPU/memory, compression/mathematics, and adversarial findings. | Portable binding record; raw specialist drafts are non-normative local archive material. |
| [How AI Discovered a Faster Matrix Multiplication Algorithm](https://youtu.be/fDAPJ7rvcUw) | Video shared in the session; led to the AlphaTensor/algorithm-discovery discussion. | Explanatory media; the Nature paper is the research source. |

## llama.cpp, GGML, and GGUF

| Source | Why it matters | PrismInfer use |
|---|---|---|
| [llama.cpp repository](https://github.com/ggml-org/llama.cpp) | Primary runtime and GGUF/GGML integration target. | Pin by commit; retain upstream default as baseline/fallback. |
| [PrismInfer's current pinned llama.cpp commit](https://github.com/ggml-org/llama.cpp/commit/ef2d770117db45b05aa7ecd1b0acca36370c5470) | Current repository compatibility pin reported by the council. | Reconcile all master-branch documentation with this exact revision. |
| [Pinned `llama.h`](https://github.com/ggml-org/llama.cpp/blob/ef2d770117db45b05aa7ecd1b0acca36370c5470/include/llama.h) | Public model/context parameters, device controls, and tensor buffer overrides. | Basis for the in-process Level C adapter. |
| [Pinned `ggml-backend.h`](https://github.com/ggml-org/llama.cpp/blob/ef2d770117db45b05aa7ecd1b0acca36370c5470/ggml/include/ggml-backend.h) | Buffers, scheduler, tensor copies, reservation, async compute, and synchronization. | Observe and apply supported behavior before proposing hooks. |
| [Current llama.cpp CLI reference](https://github.com/ggml-org/llama.cpp/blob/master/tools/cli/README.md) | Documents current `--fit`, tensor overrides, GPU layers, KV types, mmap/direct IO, NUMA, and speculation controls. | Novelty check only until each option is verified in the pin. |
| [llama-bench](https://github.com/ggml-org/llama.cpp/blob/master/tools/llama-bench/README.md) | Machine-readable upstream performance baseline. | Calibration and comparison source. |
| [Pinned GGML CUDA MMQ selector/implementation](https://github.com/ggml-org/llama.cpp/blob/ef2d770117db45b05aa7ecd1b0acca36370c5470/ggml/src/ggml-cuda/mmq.cu) | Shows that internal CUDA dispatch already exists. | Per-op PrismInfer selection requires an explicit narrow hook; not available from the CLI. |
| [Pinned GGML CUDA flash-attention path](https://github.com/ggml-org/llama.cpp/blob/ef2d770117db45b05aa7ecd1b0acca36370c5470/ggml/src/ggml-cuda/fattn.cu) | Attention implementation and internal selection. | Upstream baseline; no attention novelty claim from simply enabling it. |
| [GGUF specification](https://github.com/ggml-org/ggml/blob/master/docs/gguf.md) | Container metadata, tensor layout, alignment, and reproducibility boundary. | Source artifact remains immutable and hash-bound. |

## Windows Process, Host, and WDDM Evidence

| Source | Authoritative behavior | PrismInfer requirement |
|---|---|---|
| [Microsoft `CreateProcessW` documentation](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw) | Defines the native Unicode process-creation API, explicit application path, mutable command line, environment/current directory, handle inheritance, and returned process/thread handles. Its security notes warn about ambiguous executable parsing when the application name is omitted. | Replace shell/`std::system` launch with an explicit executable path, correctly encoded argv, controlled environment/current directory, restricted inherited handles, and retained child handle. |
| [Microsoft Job Objects documentation](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects) | A Job manages a process group as a unit, normally includes descendants, supports limits/termination, and retains basic and IO accounting even for terminated members. | Assign the baseline child/tree to a Job, prevent untracked breakaway, retain lifetime/exit accounting, and use Job recovery/cleanup semantics. |
| [`QueryInformationJobObject`](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-queryinformationjobobject) | Retrieves Job accounting and limit information. | Use Job/process APIs for child-tree evidence; never infer a child peak from the PrismInfer parent. |
| [Microsoft `GetPerformanceInfo` documentation](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getperformanceinfo) and [`PERFORMANCE_INFORMATION`](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-performance_information) | Expose system-wide physical page counts, system commit total/limit, and page size. | Convert page counts with checked arithmetic and use `CommitLimit - CommitTotal` as authoritative system commit headroom for T-101 admission. |
| [Microsoft `MEMORYSTATUSEX` documentation](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-memorystatusex) | Documents physical-memory fields and states that the total/available pagefile values are bounded by the calling process's maximum commit rather than being an unqualified system-wide commit-limit pair. | Do not derive the T-101 system commit decision from `ullTotalPageFile`/`ullAvailPageFile`; retain an explicit counter-source field and fail closed on weaker input. |
| [`IDXGIAdapter3::QueryVideoMemoryInfo`](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiadapter3-queryvideomemoryinfo) and [`DXGI_QUERY_VIDEO_MEMORY_INFO`](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/ns-dxgi1_4-dxgi_query_video_memory_info) | Expose per-memory-segment-group budget, current usage, available reservation, and current reservation for the calling process. | Record local/non-local budget and usage through the run; distinguish owned-allocation cap evidence from the stronger no-oversubscription/physical-residency claim. |
| [NVIDIA NVML device-memory query documentation](https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html) | NVIDIA states that under WDDM most device memory is allocated and managed on startup by Windows; unlike Linux/TCC, reported used memory is not defined as the sum of active-channel allocations. | NVML is corroborating evidence under WDDM, not process-residency proof. Reconcile it with DXGI, CUDA/backend allocators, workspaces, pools, and `cudaMemGetInfo`. |

## GPU Programming, Libraries, and Profiling

| Source | Contribution | Boundary for PrismInfer |
|---|---|---|
| [CUDA Programming Guide](https://docs.nvidia.com/cuda/cuda-programming-guide/) | CUDA execution, memory hierarchy, streams/events, graphs, page-locked memory, and stream-ordered allocation. | API use is not proof of overlap, capacity, or speed; retain measurements. |
| [CUDA C++ Best Practices Guide](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/) | Transfer, coalescing, occupancy, and measurement guidance. | Use as methodology, not a performance estimate. |
| [cuBLAS 13.3 / cuBLASLt](https://docs.nvidia.com/cuda/cublas/index.html) | Flexible layouts/types, algorithm heuristics, plan reuse, workspace controls, and cache. | Vendor baseline and candidate; calibration includes conversion/workspace. |
| [CUDA 13.x release notes](https://docs.nvidia.com/cuda/cuda-toolkit-release-notes/index.html) | Current library features and known issues, including autotuning and Blackwell-specific caveats. | Toolkit/library version is part of the key. |
| [CUTLASS](https://github.com/NVIDIA/cutlass) | Template kernels, profiler, generated candidate space, and Blackwell support. | Instantiate a measured subset; do not compile or benchmark an unbounded catalog. |
| [Triton matrix multiplication tutorial](https://triton-lang.org/main/getting-started/tutorials/03-matrix-multiplication.html) | Demonstrates shape-parameterized kernels and autotuning. | Isolated candidate lane; Windows/runtime integration must beat C++/CUDA baseline. |
| [FlashInfer](https://docs.flashinfer.ai/) | Plan/run separation, attention and serving kernels. | Design/baseline reference, not an MVP dependency. |
| [CUPTI](https://docs.nvidia.com/cupti/) | CUDA activity correlation and profiling APIs. | Calibration-only tracing can allocate buffers/drop records; record overhead. |
| [Nsight Systems](https://docs.nvidia.com/nsight-systems/) | CPU/GPU/transfer critical-path timelines. | Required for overlap/queue claims. |
| [Nsight Compute](https://docs.nvidia.com/nsight-compute/) | Kernel roofline, cache, occupancy, stalls, and hardware-path evidence. | Required for Tensor Core/occupancy/DRAM-bound claims. |
| [NVIDIA GPU compute capability table](https://developer.nvidia.com/cuda/gpus) | Architecture target identification. | GPU architecture alone does not make a portable plan key. |
| [RTX 50-series laptop specifications](https://www.nvidia.com/en-us/geforce/laptops/50-series/) | Official device-class specifications. | Laptop power/thermal and effective bandwidth must be measured locally. |

## Matrix Multiplication and Kernel Research

| Source | Contribution | Council decision |
|---|---|---|
| [AlphaTensor: Discovering faster matrix multiplication algorithms with reinforcement learning](https://doi.org/10.1038/s41586-022-05172-4) | Discovers exact algorithms and includes hardware-aware search. | Confirms multiple algorithms and a long-term discovery lane; not an MVP dependency or q4 decode result. |
| [Google DeepMind AlphaTensor overview](https://deepmind.google/blog/discovering-novel-algorithms-with-alphatensor/) | Primary project explanation and hardware-aware framing. | Context for the shared video. |
| [AlphaTensor code](https://github.com/google-deepmind/alphatensor) | Reproducible algorithm corpus and verification material. | Later offline research after real LLM shape corpus exists. |
| [Marlin](https://github.com/IST-DASLab/marlin) | Optimized FP16 x INT4 autoregressive matmul kernel. | Candidate/baseline only for compatible format, shapes, architecture, and fair conversion cost. |
| [T-MAC](https://arxiv.org/abs/2407.00088) | Lookup-table low-bit CPU inference without ordinary dequantized multiplication. | Important CPU candidate if exact GGUF and AVX2 path are compatible; paper speedups are not portable. |
| [QQQ](https://arxiv.org/abs/2406.09904) | W4A8 quantization plus kernels addressing prefill/decode trade-offs. | Representation-and-kernel co-design precedent. |
| [QServe](https://arxiv.org/abs/2405.04532) | W4A8KV4 algorithm/system co-design and dequantization-overhead analysis. | Demonstrates that nominal bit width and serving speed differ. |

## Heterogeneous Placement, Offload, and Scheduling

| Source | Contribution | Council use |
|---|---|---|
| [FlexGen](https://arxiv.org/abs/2303.06865) | Optimization over GPU/CPU/disk placement and access schedules. | Core precedent for an offline constrained planner; target/workload differs. |
| [HeteGen](https://arxiv.org/abs/2403.01164) | Heterogeneous parallel CPU/GPU inference. | Profiling and overlap precedent. |
| [APEX](https://arxiv.org/abs/2506.03296) | Profiling-informed parallel CPU/GPU execution on constrained GPUs. | Directly supports separate CPU/GPU time models and overlap; exact results do not transfer. |
| [PowerInfer](https://arxiv.org/abs/2312.12456) | Consumer-GPU inference using activation locality and CPU/GPU placement. | Supports activation-aware placement as a bounded precedent; validate model activation assumptions. |
| [PowerInfer-2](https://arxiv.org/abs/2406.06282) | Neuron-cluster execution/storage scheduling on smartphones. | Supports structured hardware-sized units; not proof for unmodified Ornith/SwiGLU or this PC. |
| [LLM in a Flash](https://arxiv.org/abs/2312.11514) | Storage-aware inference, contiguous loading, and reducing transferred bytes. | NVMe/flash research precedent; addressability is not latency evidence. |
| [Dynamic KV placement in heterogeneous memory](https://arxiv.org/abs/2508.13231) | Formal dynamic KV placement and theoretical bounds. | Mathematical/reference lane for living KV; evaluated memory systems differ. |
| [INFERCEPT](https://arxiv.org/abs/2402.01869) | Preserve/swap/recompute scheduling for interrupted inference. | Useful scheduling/preemption precedent for later serving workloads. |
| [PagedAttention / vLLM](https://arxiv.org/abs/2309.06180) | KV block management and reduced fragmentation. | KV management reference; does not solve weight residency. |
| [vAttention](https://arxiv.org/abs/2405.04437) | Virtual-memory-based dynamic KV management without software paging. | Later KV virtual-memory research, not a Phase 7 dependency. |

## Speculative Decoding and Transfer Amortization

| Source | Contribution | Council use |
|---|---|---|
| [Fast Inference from Transformers via Speculative Decoding, PMLR paper page](https://proceedings.mlr.press/v202/leviathan23a.html) and [Algorithm 1 PDF](https://proceedings.mlr.press/v202/leviathan23a/leviathan23a.pdf) | Exact speculative sampling preserves the target output distribution. Each verification step returns the accepted draft prefix plus one token sampled from the target or adjusted target distribution, so it commits between 1 and gamma+1 output tokens. | Define primary reward as committed target-distributed output tokens per cycle and per observed external byte. Accepted-draft length/rate is diagnostic and cannot be the sole numerator. |
| [SpecOffload](https://arxiv.org/abs/2505.10259) | Couples speculative decoding with offloaded target execution and placement planning. | Direct transfer-amortization precedent. PrismInfer must evaluate committed target-distributed output tokens per observed external byte and benchmark upstream speculation/offload first. |
| [SubSpec](https://arxiv.org/abs/2509.18344) | Substitute speculative decoding for offloaded models with target-layer sharing/alignment. | Later candidate; include substitute construction, memory, acceptance, and verification cost. |

## Weight Quantization and Compression

| Source | Contribution | Council use |
|---|---|---|
| [AWQ](https://arxiv.org/abs/2306.00978) | Activation-aware weight quantization. | Shows data-aware distortion matters; not a dynamic runtime codec by itself. |
| [AQLM](https://arxiv.org/abs/2401.06118) | Additive vector quantization for extreme weight compression. | Rate/quality candidate requiring its own decoder/kernels/artifact. |
| [QuIP#](https://arxiv.org/abs/2402.04396) | Incoherence processing and lattice codebooks. | Structured quantization precedent; not GGUF-q4 compatible automatically. |
| [QTIP](https://arxiv.org/abs/2406.11235) | Trellis quantization with incoherence processing. | Derived representation research. |
| [SpQR](https://arxiv.org/abs/2306.03078) | Sparse-quantized representation with outlier handling. | Metadata/outlier/effective-bit precedent. |
| [Any-Precision LLM](https://arxiv.org/abs/2402.10517) | Multiple precision levels from a shared/nested representation. | Supports progressive precision, but requires a derived format. |
| [BitStack](https://arxiv.org/abs/2410.23918) | Scalable bit-plane/residual-like model representation. | Strong precedent for base-plus-residual artifacts; requires local kernel/quality evidence. |
| [NeuZip](https://arxiv.org/abs/2410.20650) | Lossless neural-weight compression exploiting floating-point structure. | Storage/cold-transfer diagnostic; not proof that q4 hot tiles compress profitably. |
| [ZipNN](https://arxiv.org/abs/2411.05239) | Lossless compression for neural-network artifacts. | Same boundary: measure random access and decode cost. |
| [DP-LLM](https://arxiv.org/abs/2508.06041) | Runtime layer-wise precision selection with a learned estimator/fine-tuning. | Supports dynamic precision feasibility and establishes that selector adaptation is part of the method. |

## KV Quantization, Retention, and Progressive Transfer

| Source | Contribution | Council use |
|---|---|---|
| [KIVI](https://arxiv.org/abs/2402.02750) | Asymmetric 2-bit key/value quantization with residual window. | First concrete offline/runtime KV candidate. |
| [KVQuant](https://arxiv.org/abs/2401.18079) | Pre-RoPE key handling, non-uniform quantization, and outliers. | Alternative isolated candidate and quality methodology. |
| [H2O](https://arxiv.org/abs/2306.14048) | Heavy-hitter-oriented KV retention. | Retention lane after exact accounting. |
| [SnapKV](https://arxiv.org/abs/2404.14469) | Prompt-observation-based KV selection. | Importance-based retention precedent; validate retrieval failures. |
| [PyramidKV](https://arxiv.org/abs/2406.02069) | Layer-wise pyramidal KV budgeting. | Structured retention policy candidate. |
| [MiniCache](https://arxiv.org/abs/2405.14366) | Cross-layer/depth KV compression. | Separate research lane; do not combine initially. |
| [Dynamic Memory Compression](https://arxiv.org/abs/2403.09636) | Learned online KV merging through continued pretraining. | Establishes model-adaptation requirement for that mechanism. |
| [PolarQuant](https://arxiv.org/abs/2502.02617) | Transformed/polar low-bit vector/KV representation. | Offline dot-product/error research before a hot path. |
| [TurboQuant](https://arxiv.org/abs/2504.19874) | Fast vector/KV quantization and inner-product preservation. | Offline and then exact-cell candidate; not weight-residency solution. |
| [QJL](https://arxiv.org/abs/2406.03482) | Quantized Johnson-Lindenstrauss transforms for KV compression. | Sketch/inner-product reference with residual/error overhead. |
| [Lynx](https://arxiv.org/abs/2607.01831) | Progressive Anchor/Residual KV transfer with speculative use and verification. | Highly relevant July 2026 preprint; local PCIe replication target, not an MVP assumption. |
| [DeepSeek-V2 / Multi-head Latent Attention](https://arxiv.org/abs/2405.04434) | Model-architecture-level KV compression through MLA. | Later model co-design reference; cannot retrofit arbitrary GGUF with a runtime flag. |

## Contextual Sparsity and Conditional Depth

| Source | Contribution | Council use |
|---|---|---|
| [DejaVu](https://arxiv.org/abs/2310.17157) | Hidden-state/context-conditioned prediction of attention/MLP sparsity. | Main precedent for layer-local routing; exact model/hardware assumptions must be retested. |
| [Wanda](https://arxiv.org/abs/2306.11695) | Activation-aware pruning of LLM weights. | Structured/offline pruning reference, not automatic query routing. |
| [ProSparse](https://arxiv.org/abs/2402.13516) | Promotes activation sparsity through training. | Evidence that profitable sparsity may require model adaptation. |
| [Mixture-of-Depths](https://arxiv.org/abs/2404.02258) | Trains token-level routing under a compute budget. | Whole-layer/depth decisions are model-runtime co-design. |
| [LayerSkip](https://arxiv.org/abs/2404.16710) | Layer dropout and early-exit/speculative self-decoding. | Full layer skipping is an adaptation lane, not a training-free runtime switch. |

## Information Theory and Optimization

| Source | Contribution | Council use |
|---|---|---|
| [Shannon, A Mathematical Theory of Communication](https://doi.org/10.1002/j.1538-7305.1948.tb01338.x) | Entropy and lossless coding limits. | Effective-bit and entropy-census framing. |
| [Shannon, Coding Theorems for a Discrete Source With a Fidelity Criterion](https://ieeexplore.ieee.org/document/5311476) | Rate-distortion formulation. | Representation quality/bit Pareto view; task distortion remains empirical. |
| [Ansor: Generating High-Performance Tensor Programs](https://www.usenix.org/conference/osdi20/presentation/zheng) | Measurement-driven tensor-program search and cost modeling. | General autotuning precedent; PrismInfer's search space includes runtime placement and evidence constraints. |

## Language and Toolchain Sources

| Source | Contribution | Council decision |
|---|---|---|
| [Mojo system requirements](https://docs.modular.com/mojo/requirements/) | Officially says Mojo does not natively support Windows and runs there through WSL; RTX 50-series is listed as known-compatible GPU hardware. | No native-Windows PrismInfer runtime dependency. A WSL result is a different OS/runtime validation cell. |
| [Mojo roadmap](https://docs.modular.com/mojo/roadmap/) | High-performance CPU/GPU coding remains in progress; Phase 2 application-level systems programming is marked not started. | Keep C++20/CUDA as production runtime and treat Mojo as an isolated operator-provider experiment. |
| [Mojo `ffi` standard-library module](https://docs.modular.com/mojo/std/ffi/) | Provides C types, external C calls, and dynamic-library loading. | Do not claim native interoperability is absent; constrain experiments to the versioned provider ABI and record toolchain/runtime versions. |
| [Mojo `@export` documentation](https://docs.modular.com/mojo/manual/decorators/export/) | Supports exported symbols and currently a C ABI specifier. | A Mojo prototype may implement a narrow C ABI `KernelProvider`; C FFI/C ABI availability does not make Mojo a mature Windows application runtime. |

## Model Candidates and Architecture Evidence

| Source | Relevance | Selection boundary |
|---|---|---|
| [Meta Llama 3.1 8B Instruct official model card](https://huggingface.co/meta-llama/Llama-3.1-8B-Instruct) | Text-only autoregressive transformer using GQA; the final Plan's preferred foundation cell. | Selection remains conditional on accepted license/access, exact revision, proven pinned llama.cpp conversion/load support, self-produced quantization, and immutable artifact identity. |
| [Ornith-1.0-9B model card](https://huggingface.co/deepreinforce-ai/Ornith-1.0-9B) | Proposed high-capability coding/agent cell; identifies the `qwen3_5` multimodal family and MIT license. | Secondary capability and hybrid multimodal stress cell, not the controller foundation. Pin revision and scope text-only versus multimodal artifacts separately. |
| [Ornith-1.0-9B official `config.json`](https://huggingface.co/deepreinforce-ai/Ornith-1.0-9B/blob/main/config.json) | Declares `Qwen3_5ForConditionalGeneration`, 32 text layers with `full_attention_interval=4`, an explicit linear/full layer list, one MTP layer, and a vision configuration. | Architecture-state ledger must separate 8 full-attention layers, 24 linear-attention layers, convolution/recurrent state, MTP, and optional vision/projector memory. Uniform 32-layer KV math is invalid. |
| [Qwen3.5-9B official model card](https://huggingface.co/Qwen/Qwen3.5-9B) | Official lineage and multimodal behavior for Ornith's base architecture. | Qwen3.5 is a lineage comparator, not an independent conventional architecture control. |
| [Qwen3.5-9B official `config.json`](https://huggingface.co/Qwen/Qwen3.5-9B/blob/main/config.json) | Independently exposes the same 24-linear/8-full attention schedule, linear-state dimensions, MTP layer, and vision configuration. | Use to define architecture/operator coverage and state accounting; do not generalize full-attention KV-compression results to linear state. |
| [Qwen3.5 Transformers architecture documentation](https://huggingface.co/docs/transformers/model_doc/qwen3_5) | Documents hybrid attention/DeltaNet structure and implementation requirements. | Verify exact llama.cpp support; do not assume a generic Llama-like graph. |
| [Gemma 2 9B IT model card](https://huggingface.co/google/gemma-2-9b-it) | Candidate ordinary dot-product-attention 9B control. | Alternating local/global attention, gated access/license, and artifact production must be recorded; do not call it globally full-attention without the exact architecture qualifier. |

## Source-Use Rules

- Prefer the paper, official documentation, or official repository over a
  secondary summary.
- Record access date and version/commit in implementation manifests.
- Treat current upstream `master` as a novelty/compatibility check only; it does
  not prove that the pinned runtime exposes the same actuator.
- Do not import paper speedups into PrismInfer estimates.
- Treat 2025/2026 preprints, especially Lynx and newer dynamic systems, as
  emerging replication targets until independently validated.
- Separate model-architecture methods such as MLA, Mixture-of-Depths, or
  continued-pretraining KV merging from runtime-only mechanisms.
- Every representation requires an exact artifact, decoder/kernel, effective
  bit accounting, quality fixture, and lifecycle-specific recovery class.
- Speculative systems are compared using committed target-distributed output
  tokens per cycle and per observed external byte; draft acceptance is a
  diagnostic.
- Every systems result is exact-cell evidence, never a bucket-wide conclusion.
