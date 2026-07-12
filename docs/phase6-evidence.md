# Phase 6 Evidence

Phase 6 is partially implemented as schema, comparator, policy, and synthetic
CUDA scaffolding. It has no model-backed evidence and remains `research-only`.

Current status:

- Strict kernel-manifest file ingestion and same-cell comparator tests exist.
- The CPU-only `prism-emit-benchmark` path canonicalizes manifests, writes a
  deterministic SHA-256 sidecar, and fails closed when completed outcomes lack
  raw-trial evidence or non-completed outcomes lack failure evidence. It also
  requires timing, device-residency, host-commit, supervisor/admission, and
  zero-unknown-owned-byte fields; this contract is not model-run evidence.
- `configs/model-cell-catalog.json` pins only the checked-in deterministic
  smoke fixture and records a source-verified, non-admitted foundation receipt;
  Ornith stress and Qwen lineage entries remain metadata-only with no fetched
  artifact identity.
- The Phase 6 gate schema/config and compression-oriented manifest fields exist.
- The guarded CUDA target, bounded synthetic CUDA correctness test source,
  `-WithCudaKernels` verification flag, and manual self-hosted workflow exist.
- The CUDA fixture uses toy `Q4Block` semantics; it is not evidence for any
  selected GGUF tensor type or model.
- The preferred foundation source is hash-verified and a self-produced F16
  intermediate is retained outside the repository; four Q4_K_M attempts were
  aborted at run-specific CPU working-set bounds (4 GiB, 6 GiB, 8 GiB, and
  10 GiB). The authorized 15 GiB retry's retained receipt reports that the 4 GiB
  physical-reserve guard fired, but lacks the sampled physical-memory values
  needed to independently prove that inequality; it is therefore an explicitly
  unverified, noncanonical forensic receipt. No canonical foundation quant exists.
- The source-verified foundation receipt pins source config, tokenizer,
  tokenizer-template, license/use-policy, architecture, converter, recipe,
  and imatrix-not-used metadata. Its retained F16 metadata inventory contains
  292 tensors (226 F16 and 66 F32); these records are source evidence only and
  are not a Q4_K_M tensor-type inventory.
- No same-cell llama.cpp/GGML CUDA/MMQ baseline exists.
- No manifest-backed PrismInfer candidate kernel benchmark exists.
- No compression-specific foundation/stress-cell benchmark manifest exists.
- No retained KV tensor compression fixture exists.
- No offline PolarQuant, TurboQuant, QJL, KIVI, KVQuant, or QServe-style
  evaluator result exists for an exact selected validation cell.
- No compression-aware memory ledger evidence exists for resident q4 weights,
  KV payload, KV metadata, residual/sketch bytes, reconstruction workspace,
  retained pools, and unknown bytes.
- No Phase 6 validated, deployable, foundation, stress-cell, or bucket-wide
  claim is made.
- [#103](https://github.com/Gravelaw/prisminfer/issues/103) blocks model-backed
  CUDA, calibration, and evidence until the hardware supervisor and pre-context
  admission boundary pass.

Model-backed Phase 6 evidence will be added only after #103, P7-01 model
selection, and the remaining stages in
`docs/phase6-implementation-plan.md` and the workflow in
`docs/phase6-compression-architecture.md` are implemented and verified.

Council-reviewed claim boundary:

```text
No selected-foundation or 9B-stress constrained-inference claim yet.
No custom-kernel speedup claim.
No deployable-profile claim.
No Tensor Core claim.
No bucket-wide >5B-10B claim from one model.
No constrained-VRAM claim if full FP16 weights are materialized.
```

The preferred first evidence target is a P7-01-pinned Meta Llama 3.1 8B
foundation GGUF, pending license acceptance, access, exact revision, converter
support, reproducible quantization, and hashes. Ornith-1.0-9B is a separate
hybrid stress cell. Gemma 2, if retained as an optional comparison, is described
by its actual sliding-window/global-attention pattern rather than as globally
full-attention. Any result remains exact-model, exact-recipe, exact per-tensor
`ggml_type`, exact-context, exact-hardware, and exact-cap scoped.

Planned evidence architecture:

```text
#103 supervisor/admission clearance
  -> P7-01 pinned foundation GGUF
  -> quantization recipe plus exact per-tensor ggml_type inventory
  -> upstream quantized resident-weight baseline
  -> no full FP16 materialization gate
  -> KV ledger
  -> complete memory ledger
  -> quality and performance gates
  -> same-cell comparator
  -> core Phase 6 classification
  -> optional custom-kernel candidate and exact tensor reference
  -> optional KV-compression candidate and reconstruction evidence
  -> independent optional classifications
```

The core result does not require a custom-kernel speedup or KV-compression win.
If either optional hypothesis is claimed, its evidence remains `research-only`
until the implementation records its applicable exact semantics, effective
bits/metadata/workspace, quality, end-to-end timing, and cap certification.

Until then, Phase 6 status is:

```text
research-only
```

Required evidence before this file can move beyond `research-only`:

- #103 clearance and retained supervisor/admission evidence,
- P7-01 model-selection, license, source, conversion, and quantization record,
- exact GGUF model hash and quant artifact hash,
- quantization recipe and per-tensor `ggml_type` manifest hash,
- tokenizer and prompt fixture hashes,
- CPU/reference correctness result,
- llama.cpp/GGML same-quant baseline,
- llama.cpp/GGML CUDA/MMQ same-cell baseline,
- PrismInfer no-custom baseline,
- PrismInfer q4 candidate manifest only when a custom-kernel claim is tested,
- exact selected-tensor reference fixture only when a custom path consumes it,
- compression candidate manifest, effective-bit/metadata evidence, KV
  reconstruction timing, attention-logit error, and top-k overlap only when KV
  compression is tested,
- quality fixture result,
- lifecycle validator result,
- cap-certification result or explicit `measured-non-certified` reason,
- retained profiler artifact only when a hardware-path claim is made.
