# KV Cache and Compression Research

This note records the current PrismInfer research direction for KV cache memory,
quantization, and alternative numeric representations.

## Why KV Cache Matters

For autoregressive inference, KV cache memory grows with context length and
batch size. A simplified estimate is:

```text
M_KV ~= 2 * B * T * L * H_kv * D * bits_kv / 8 + M_metadata
```

Where:

- `B` is batch size.
- `T` is context length.
- `L` is layer count.
- `H_kv` is KV head count.
- `D` is head dimension.
- The factor `2` represents keys and values.

For grouped-query attention, `H_kv` may be much smaller than the number of query
heads. PrismInfer should read actual GGUF/model metadata instead of inferring
from model family names.

## Paged KV Cache

PagedAttention and vLLM show that KV cache should be managed like a block-based
memory system rather than one large contiguous allocation.

PrismInfer should borrow the accounting model before building kernels:

- logical token count,
- KV block size in tokens,
- bytes per block,
- active block count,
- reusable block count,
- evicted block count,
- prefix-shared block count,
- peak KV bytes,
- cache hit/miss/promote/evict counters.

Phase 1 captures backend warmup and unknown allocation evidence. Phase 2 owns
the KV block ledger, paging/reuse/offload accounting, and compression policy.

Current implementation status:

- `none` and `accounting-only` are governed policy labels.
- `reference` can be evaluated only when a quality gate is present.
- KIVI/KVQuant, PolarQuant, TurboQuant, and QJL remain metadata-only research
  lanes until actual implementation and task-level quality evidence exist.
- A compression claim with no quality gate is rejected with
  `quality_gate_required_for_compression`.

## Asymmetric KV Quantization

KIVI-style work suggests keys and values should not necessarily use the same
quantization axis.

Track separately:

```text
key_quant_axis
value_quant_axis
pre_rope_or_post_rope
key_bit_width
value_bit_width
group_size
residual_fp_window_tokens
outlier_threshold
metadata_bits_per_value
effective_bits_per_value
```

Initial experiment:

- keys: per-channel quantization,
- values: per-token quantization,
- small high-precision residual window,
- no custom CUDA kernel until backend memory governance exists.

## PolarQuant, TurboQuant, and QJL

PolarQuant and TurboQuant-style methods are relevant because attention depends
on dot products, not just reconstruction mean-squared error.

Useful concepts:

```text
y = R x
y_hat = Q(y)
x_hat = R^T y_hat
```

Where `R` is a random or structured rotation and `Q` is a low-bit quantizer.

QJL-style residual sketches can reduce inner-product bias by storing a compact
sign sketch of the residual.

PrismInfer should track:

```text
rotation_kind
rotation_seed
projection_family
projected_dimension
main_quantizer_bits
residual_bits
norm_storage_bits
estimator_bias
estimator_variance
attention_logit_error_p95
attention_logit_error_p99
attention_topk_overlap
```

Guardrail:

Do not claim TurboQuant or PolarQuant safety until PrismInfer has task-level
quality gates and implementation-specific decode overhead measurements.

Nominal bit width is not enough. A `2-bit`, `3-bit`, or `4-bit` claim counts
only when effective bits, metadata overhead, decode overhead, and task deltas
are reported for the exact validation cell.

## Other Bit and Byte Representations

Candidate representations:

- GGML/GGUF block quantization families such as `Q4_K`, `Q5_K`, and `IQ*`.
- FP8, FP4, NF4, and MX-style formats where hardware/software support exists.
- 2-bit and 3-bit KV formats.
- Product quantization and vector quantization.
- Structured rotations before scalar quantization.
- Sparse outlier side channels.
- Entropy coding for cold/offline storage.

Hot-path caution:

Entropy coding can reduce average bits, but it can also destroy random access,
coalescing, and predictable decode latency. It is more plausible for cold weight
storage or CPU staging than for hot GPU KV cache.

## Quality Gates

Perplexity is not enough. A compression path must be evaluated against:

- baseline token divergence at temperature 0,
- perplexity delta,
- attention logit error distribution,
- attention top-k overlap,
- needle-in-a-haystack retrieval,
- LongBench/RULER-style long-context tasks,
- task-specific prompt suite,
- time-to-first-token,
- prefill tokens/sec,
- decode tokens/sec,
- certified peak memory.

Reject a compression profile if retrieval or task quality fails even when
perplexity appears acceptable.

## Manifest Fields To Add Later

```text
quantization_scope
algorithm_family
payload_bits_per_value
effective_bits_per_value
metadata_bits_per_value
quant_axis
scale_policy
zero_point_policy
codebook_policy
outlier_policy
rotation_policy
projection_policy
quality_gate_id
calibration_dataset_id
quant_artifact_sha256
decode_policy
cap_certification_status
```

## Research Sources

- PagedAttention: https://arxiv.org/abs/2309.06180
- KIVI: https://arxiv.org/abs/2402.02750
- KVQuant: https://arxiv.org/abs/2401.18079
- PolarQuant: https://arxiv.org/abs/2502.02617
- TurboQuant: https://arxiv.org/abs/2504.19874
- Google Research TurboQuant summary: https://research.google/blog/turboquant-redefining-ai-efficiency-with-extreme-compression/
