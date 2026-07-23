# Phase 6 Evidence

Phase 6 is partially implemented as schema, comparator, policy, and synthetic
CUDA scaffolding. It has no model-backed evidence and remains `research-only`.

Current status:

- Strict kernel-manifest file ingestion and same-cell comparator tests exist.
- The CPU-only `prism-emit-benchmark` path canonicalizes manifests, writes a
  deterministic SHA-256 sidecar, and fails closed when completed outcomes lack
  raw-trial evidence or non-completed outcomes lack failure evidence. It also
  requires an explicit trusted output root, rejects non-completed outcomes that
  retain raw trials or promotion state, and requires timing, device-residency,
  host-commit, supervisor/admission, and zero-unknown-owned-byte fields; this
  contract is not model-run evidence.
  The normative evidence trust contract is owned by
  `docs/adaptive-runtime-v2/evidence-thresholds-and-security.md`.
- `configs/model-cell-catalog.json` pins the checked-in deterministic smoke
  fixture and the verified Llama 3.1 8B Q4_K_M foundation artifact identity
  completed by #80. The foundation bytes were admitted under the recorded
  one-time related-community-artifact exception; this is not self-produced
  provenance or model-execution admission. Ornith remains an immutable
  unsupported-converter source descriptor, and the Qwen lineage entry remains
  metadata-only.
- The Phase 6 gate schema/config and compression-oriented manifest fields exist.
- The guarded CUDA target, bounded synthetic CUDA correctness test source,
  `-WithCudaKernels` verification flag, and manual self-hosted workflow exist.
- The CUDA fixture uses toy `Q4Block` semantics; it is not evidence for any
  selected GGUF tensor type or model.
- The completed #80 foundation record pins source config, tokenizer and template,
  license/use policy, architecture, producer/converter revisions, exact recipe,
  artifact SHA-256, and the complete admitted Q4_K_M inventory: 66 F32, 193
  Q4_K, and 33 Q6_K tensors. The producer-declared imatrix is part of that
  identity. The external bytes are usable only through the approved artifact
  root and exact retained hashes.
- The self-produced F16 intermediate and the bounded 4 GiB, 6 GiB, 8 GiB,
  10 GiB, and 15 GiB local quantization attempts remain negative provenance
  evidence. The 15 GiB attempt's physical-reserve receipt lacks the sampled
  values needed to prove its guard inequality. None of those attempts supplies
  self-produced Q4_K_M provenance or model-execution credit.
- The source-verified Ornith receipt pins the 18-file BF16 source manifest,
  config, tokenizer, tokenizer template, architecture, and MIT license
  evidence. It remains noncanonical pending converter/operator coverage and
  the separately required supervised stress-cell clearance.
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
- [#103](https://github.com/Gravelaw/prisminfer/issues/103) implemented the
  Packet B hardware supervisor and staged pre-context admission boundary. C2 is
  still closed: the existing #119 terminal attempts are non-promotable and grant
  no hardware or model-execution credit.

Model-backed Phase 6 evidence can be added only after a fresh exact-SHA hardware
authorization and independently accepted
[#119](https://github.com/Gravelaw/prisminfer/issues/119) C2 receipt, followed by
the retained `#84 -> #76 -> #77 -> #78` Packet C sequence in
`docs/phase6-implementation-plan.md` and
`docs/phase6-compression-architecture.md`.

Council-reviewed claim boundary:

```text
No selected-foundation or 9B-stress constrained-inference claim yet.
No custom-kernel speedup claim.
No deployable-profile claim.
No Tensor Core claim.
No bucket-wide >5B-10B claim from one model.
No constrained-VRAM claim if full FP16 weights are materialized.
```

The first conventional evidence target is the exact #80-pinned Meta Llama 3.1
8B foundation GGUF. Its accepted license/access, source and producer revisions,
recipe, hashes, and per-tensor inventory are immutable inputs, not execution or
self-production claims. Ornith-1.0-9B is a separate unsupported-converter hybrid
stress cell. Gemma 2, if retained as an optional comparison, is described by its
actual sliding-window/global-attention pattern rather than as globally
full-attention. Any result remains exact-model, exact-recipe, exact per-tensor
`ggml_type`, exact-context, exact-service-profile, exact-runtime,
exact-OS-execution-mode, exact-hardware, and exact-cap scoped. An external
runtime is always another exact cell and may support only a labelled paired-cell
comparison after the projection and pair-specific artifact-equivalence gates
pass.

Planned evidence architecture:

```text
#80 completed immutable foundation identity + #103 implemented supervisor
  -> fresh exact-SHA #119 authorization and independently accepted C2 receipt
  -> #84 exact capacity/admission
  -> #76 deterministic quality fixtures
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

- fresh exact-SHA #119 authorization, independently accepted C2 receipt, and
  retained #103 supervisor/admission evidence,
- retained #80 model-selection, license, source, producer/converter,
  quantization, and one-time acquisition-exception record,
- #84 exact capacity/admission result and #76 deterministic quality fixtures,
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
