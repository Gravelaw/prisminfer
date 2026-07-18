# Dependency Pins

Phase 1 backend-enabled runs must record exact dependency and artifact pins
before they can produce promoted backend evidence.

## Current Phase 1 Status

| Dependency | Pin status | Notes |
|---|---|---|
| llama.cpp | `ef2d770117db45b05aa7ecd1b0acca36370c5470` | Recorded in `third_party/llama.cpp-pin.json`; source is not vendored; local evidence uses an external CPU build wrapper. |
| GGML | `ef2d770117db45b05aa7ecd1b0acca36370c5470` | Recorded as part of the pinned llama.cpp repository source. |
| GGUF schema/source | `ef2d770117db45b05aa7ecd1b0acca36370c5470` | Recorded as part of the pinned llama.cpp repository source. |
| Model artifact | External only | Do not commit model weights; record model id, path, hash, and sidecar hash in retained evidence. |

## Enforcement

`prism-probe --backend llama` fails closed with `dependency_pins_required` when
no dependency pin file is supplied. Use `third_party/llama.cpp-pin.json` as the
current source pin record for opt-in integration work.

The current llama adapter is an opt-in process-backed integration. On Windows it
uses the native Job-contained worker; shell, `cmd.exe`, and environment-selected
executables are not accepted. The immutable
`third_party/llama.cpp-executable-approval.json` record pins the locally built
CPU-only static `llama.exe cli` baseline from commit `ef2d7701`, its build
identity, and executable hash. That baseline is built with shared libraries,
dynamic GGML backend loading, CUDA, and OpenSSL disabled; its PE import table
contains only Windows and Microsoft runtime DLLs. The worker resolves the
workspace-relative record, verifies the opened executable identity, keeps its
share-restricted handle through launch, and applies the child-bound
System32-preferred image-load policy. The executable hash binds the approved PE
image, including its import table. This approval records executable identity
only; it does not authorize a model-backed run. The reproducible build flags,
version, size, hash, and PE dependency inventory are retained in
`third_party/llama.cpp-static-build-receipt.md`. Until
llama.cpp allocation paths are reconciled with PrismInfer-owned/device memory
evidence, llama mode must not be described as full governed backend allocation.

Preferred claim wording before allocation reconciliation:

```text
observed backend allocation evidence
```

Disallowed claim wording:

```text
full governed backend allocation
```
