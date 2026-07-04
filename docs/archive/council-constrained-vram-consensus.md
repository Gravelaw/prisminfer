# Constrained VRAM Council Consensus

Date: 2026-07-04.

This document consolidates the second PrismInfer council pass for running LLMs
under constrained GPU memory. It checks the current repo state and converts the
research discussion into claims, gates, and implementation direction.

## Current Repo State

The current repository state supports a conservative claim:

PrismInfer is a Phase 0 telemetry and governance scaffold for constrained-memory
LLM inference experiments.

It currently provides:

- `1gb-safe-cpu` and `1gb-safe-gpu-probed` probe modes.
- A default hard cap of `1073741824` bytes.
- CUDA context probing when compiled with `PRISMINFER_ENABLE_CUDA_PROBE=ON`.
- JSONL telemetry, lifecycle validation, benchmark manifests, sidecar metadata
  validation, and forced fail-closed tests.
- Self-hosted Windows/NVIDIA/CUDA CI evidence.

It does not yet provide:

- llama.cpp, GGML, or GGUF runtime integration.
- Real model loading or token generation.
- Full backend allocation governance.
- KV cache ownership, paging, compression, or offload.
- Transfer-inclusive GPU profitability evidence.
- Validated 90B hybrid deployment evidence.

## Council Thesis

PrismInfer should be positioned as a memory-governed inference framework, not a
new inference runtime and not a claim that large dense models fit fully inside a
small GPU.

The defensible theory is:

1. Keep llama.cpp, GGML, and GGUF compatibility as the runtime substrate.
2. Treat GPU memory as a certified envelope, not a best-effort suggestion.
3. Certify only manifest-backed, lifecycle-valid runs with complete telemetry.
4. Fail closed when evidence is missing, contradictory, incomplete, or
   semantically insufficient.
5. Add performance, KV compression, offload, and 90B claims only after separate
   quality and usability gates pass.

## Allowed Claims Now

- PrismInfer can run Phase 0 probe workflows and produce hard-cap evidence for
  probe modes.
- CUDA probe evidence can show context creation, driver/runtime version, GPU
  name, and memory readings on the current self-hosted runner.
- Forced breach paths can prove fail-closed behavior without allocating excessive
  real GPU memory.
- `90b-hybrid-simulated` is a simulated or lower-bound configuration label only.

## Claims Not Allowed Yet

- PrismInfer fully governs llama.cpp or GGML allocations.
- 8B to 12B models fit in 1GB VRAM.
- 90B models are deployable on 8GB VRAM.
- GPU offload is faster than CPU.
- KV cache compression preserves useful task quality.
- Device memory delta alone proves hard-cap compliance.
- Manifest-less probe output is certified benchmark evidence.

## 8B to 12B Under 1GB VRAM

The realistic target is CPU-resident or mostly CPU-resident inference with
certified bounded GPU participation.

Under a strict 1GB VRAM cap:

- Weights are primarily in CPU memory or mmap-backed host memory.
- GPU participation is optional and must be admitted after CUDA context probing.
- GPU-resident KV cache is viable only for small contexts, partial placement, or
  compressed/paged strategies.
- Layer offload must be planned after context/runtime overhead is measured.
- Any success claim must include quality, time-to-first-token, tokens/sec, and
  memory evidence.

Preferred wording:

PrismInfer aims to make 8B to 12B inference safe and measurable under a locked
1GB VRAM envelope by treating the GPU as a bounded accelerator, not as the full
model host.

## 90B Hybrid Classification

90B fully resident inference on 8GB VRAM is not physically plausible for a dense
model. Even 1 bit per parameter requires roughly 11.25 GB for weight payload
alone, before scales, metadata, KV cache, activations, CUDA context, allocator
state, and workspaces.

Any 90B result must use one of these labels:

| Label | Meaning |
|---|---|
| `metadata-only` | Configuration, artifact, or sidecar validation only. |
| `simulated` | Lower-bound, extrapolated, or planner-only result. |
| `offload-experiment` | Real execution with CPU/NVMe/limited-GPU offload, not yet meeting usability gates. |
| `validated-benchmark` | Reproducible benchmark with model hash, hardware, memory, latency, and quality evidence. |
| `deployable-profile` | Validated benchmark plus repeatability, acceptable latency, and documented operating constraints. |

Current PrismInfer 90B language must remain simulated or offload-experiment
until validated evidence exists.

## Council Iterations

### Iteration 1: Optimistic Theory

The optimistic theory is that large-model inference can be made feasible on
lower-end GPUs by combining:

- weight quantization,
- KV cache quantization,
- KV paging,
- CPU/GPU/NVMe memory hierarchy planning,
- mmap and staged loading,
- bounded GPU buffers,
- transfer-aware scheduling,
- strict telemetry.

### Iteration 2: Devil's Advocate

The limiting factors are:

- Dense 90B resident weights do not fit in 8GB VRAM.
- Phase 0 does not load a model or generate tokens.
- Empty or no-model telemetry is not model-inference proof.
- WDDM/NVML readings can be noisy or misleading.
- CUDA context overhead can consume a material part of a 1GB profile.
- PCIe/NVMe offload can make a model technically runnable but unusably slow.
- KV compression can preserve perplexity while breaking retrieval.

### Iteration 3: Consensus

The next decisive gate is not a new kernel. It is real backend governance.

Phase 1 must attach a backend adapter, pin llama.cpp/GGML/GGUF dependencies,
replace the placeholder warmup with real backend warmup, and prove that backend
allocations are observed or bounded before runtime governance is claimed.

## Phase Gates

| Phase | Gate | Advance if | Stop if |
|---|---|---|---|
| Phase 1 | Real backend governance | llama.cpp-backed runs are manifest-backed, lifecycle-valid, cap-certified, and fail closed on hidden/unknown allocations. | Backend allocations cannot be tracked or bounded. |
| Phase 2 | Quality viability | KV compression or memory strategy passes task-level quality gates within declared tolerances. | Perplexity passes but retrieval/task quality fails. |
| Phase 3 | GPU/offload profitability | GPU-probed or offloaded mode wins transfer-inclusive benchmarks while remaining cap-certified. | Total wall-clock is worse than CPU or telemetry is incomplete. |
| Phase 4 | 90B hybrid validation | Validated 90B profile meets cap, quality, latency, and repeatability thresholds. | Evidence remains simulated, extrapolated, or unusably slow. |

## Sources

- PagedAttention: https://arxiv.org/abs/2309.06180
- KIVI: https://arxiv.org/abs/2402.02750
- FlexGen: https://arxiv.org/abs/2303.06865
- PolarQuant: https://arxiv.org/abs/2502.02617
- TurboQuant: https://arxiv.org/abs/2504.19874
- Google Research TurboQuant summary: https://research.google/blog/turboquant-redefining-ai-efficiency-with-extreme-compression/

