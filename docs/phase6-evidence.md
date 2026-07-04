# Phase 6 Evidence

Phase 6 is planned but not yet implemented.

Current status:

- Phase 5 CUDA kernel scaffold exists.
- The guarded CUDA kernel target compiles through the VS 2026 CUDA preset.
- No retained real 9B GGUF artifact exists in the repository.
- No same-cell llama.cpp/GGML CUDA/MMQ baseline exists.
- No manifest-backed PrismInfer candidate kernel benchmark exists.
- No compression-specific 9B benchmark manifest exists.
- No retained KV tensor compression fixture exists.
- No offline PolarQuant, TurboQuant, QJL, KIVI, KVQuant, or QServe-style
  evaluator result exists for an exact 9B validation cell.
- No compression-aware memory ledger evidence exists for resident q4 weights,
  KV payload, KV metadata, residual/sketch bytes, reconstruction workspace,
  retained pools, and unknown bytes.
- No Phase 6 validated, deployable, or bucket-wide 9B claim is made.

Phase 6 evidence will be added only after the stages in
`docs/phase6-implementation-plan.md` and the workflow in
`docs/phase6-compression-architecture.md` are implemented and verified.

Council-reviewed claim boundary:

```text
No 9B constrained inference claim yet.
No custom-kernel speedup claim.
No deployable-profile claim.
No Tensor Core claim.
No bucket-wide >5B-10B claim from one model.
No constrained-VRAM claim if full FP16 weights are materialized.
```

The first evidence target is one exact dense 9B-class q4 GGUF validation cell.
Any result must remain exact-model, exact-quant, exact-context, exact-hardware,
and exact-cap scoped until multiple retained representatives justify broader
claims.

Planned evidence architecture:

```text
pinned 9B GGUF artifact
  -> q4 resident weight plan
  -> no full FP16 materialization gate
  -> exact GGUF q4 reference decode
  -> CUDA launch correctness
  -> KV ledger
  -> optional governed compression policy
  -> bounded KV reconstruct/dequant hot path
  -> compression-aware memory ledger
  -> quality and performance gates
  -> same-cell comparator
  -> Phase 6 classification
```

Compression evidence remains `research-only` until the implementation records
effective bits, metadata overhead, reconstruction workspace, attention error,
top-k overlap, task-quality deltas, end-to-end timing, and cap certification for
the exact validation cell.

Until then, Phase 6 status is:

```text
research-only
```

Required evidence before this file can move beyond `research-only`:

- exact GGUF model hash and quant artifact hash,
- tokenizer and prompt fixture hashes,
- q4 tensor-slice correctness fixture,
- CPU/reference correctness result,
- llama.cpp/GGML same-quant baseline,
- llama.cpp/GGML CUDA/MMQ same-cell baseline,
- PrismInfer no-custom baseline,
- PrismInfer q4 candidate manifest,
- compression candidate manifest when compression is tested,
- compression effective-bit and metadata-overhead evidence,
- KV reconstruction workspace and timing evidence,
- attention-logit error and attention top-k overlap evidence,
- quality fixture result,
- lifecycle validator result,
- cap-certification result or explicit `measured-non-certified` reason,
- retained profiler artifact only when a hardware-path claim is made.
