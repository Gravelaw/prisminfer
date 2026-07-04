# Dependency Pins

Phase 1 backend-enabled runs must record exact dependency and artifact pins
before they can produce promoted backend evidence.

## Current Phase 1 Status

| Dependency | Pin status | Notes |
|---|---|---|
| llama.cpp | `ef2d770117db45b05aa7ecd1b0acca36370c5470` | Recorded in `third_party/llama.cpp-pin.json`; source is not vendored. |
| GGML | `ef2d770117db45b05aa7ecd1b0acca36370c5470` | Recorded as part of the pinned llama.cpp repository source. |
| GGUF schema/source | `ef2d770117db45b05aa7ecd1b0acca36370c5470` | Recorded as part of the pinned llama.cpp repository source. |
| Model artifact | External only | Do not commit model weights; record model id, path, hash, and sidecar hash in retained evidence. |

## Enforcement

`prism-probe --backend llama` fails closed with `dependency_pins_required` when
no dependency pin file is supplied. Use `third_party/llama.cpp-pin.json` as the
current source pin record for opt-in integration work.

The current llama adapter is an opt-in surface only. Until a real llama.cpp
integration is linked and reconciled with PrismInfer memory evidence, llama mode
must not be described as full governed backend allocation.

Preferred claim wording before allocation reconciliation:

```text
observed backend allocation evidence
```

Disallowed claim wording:

```text
full governed backend allocation
```
