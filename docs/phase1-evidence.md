# Phase 1 Evidence Record

This file records Phase 1 backend-governance evidence. Evidence is valid only
when the matching command output, manifest, telemetry JSONL, or GitHub Actions
artifact is retained or linked from the Phase 1 issue or PR.

## Current Implementation Baseline

Phase 1 currently provides:

- maximum GPU hard-cap policy capped at `17179869184` bytes,
- validation-cell config, telemetry, manifest, and schema fields,
- backend adapter boundary,
- null backend,
- deterministic fake backend,
- backend memory observer,
- memory ledger,
- lifecycle v0.2 backend events,
- benchmark manifest v0.2,
- minimum host-memory and process-IO manifest fields,
- pinned llama.cpp/GGML/GGUF source baseline in
  `third_party/llama.cpp-pin.json`,
- opt-in llama backend surface that fails closed until real integration exists.

Phase 1 does not yet provide:

- linked llama.cpp/GGML/GGUF source integration,
- real GGUF model loading,
- real llama.cpp warmup,
- reconciled llama.cpp allocation governance,
- representative `<=2B` and `>2B-5B` real backend warmup artifacts.

## Local Verification

Default verification passed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1
```

Result:

```text
9/9 CTest tests passed.
CPU probe smoke passed lifecycle validation.
Forced allocator/process/warmup/unknown breach matrix passed lifecycle validation.
```

Opt-in llama backend surface check passed in the standard build directory:

```powershell
cmake -S . -B build -DPRISMINFER_ENABLE_LLAMA_BACKEND=ON
cmake --build build --config Debug --target prism-probe prism-validate-lifecycle test_backend_adapter
ctest --test-dir build -C Debug -R test_backend_adapter --output-on-failure
```

Result:

```text
test_backend_adapter passed.
```

Opt-in llama mode with the pinned dependency file fails closed because the real
llama.cpp adapter is not implemented yet:

```powershell
.\build\Debug\prism-probe.exe `
  --backend llama `
  --dependency-pin-file third_party\llama.cpp-pin.json `
  --telemetry phase1-llama-pinned.jsonl `
  --manifest phase1-llama-pinned-manifest.json `
  --run-id phase1-llama-pinned
.\build\Debug\prism-validate-lifecycle.exe phase1-llama-pinned.jsonl
cmake -S . -B build -DPRISMINFER_ENABLE_LLAMA_BACKEND=OFF
```

Result:

```text
status: failed_closed
failure_reason: llama_backend_not_implemented
lifecycle valid: true
backend: llama
```

Note: a separate `build-llama` directory CTest run was blocked by Windows
Application Control for one generated test executable. The opt-in build path
was therefore verified in the standard `build` directory and the generated
`build-llama` directory was removed.

## Backend Evidence Artifacts

Fake backend warmup evidence:

```powershell
.\build\Debug\prism-probe.exe `
  --backend fake `
  --warmup-tokens 8 `
  --model-parameter-bucket '<=2B' `
  --parameter-count 1000000 `
  --vram-tier-gib 1 `
  --validation-cell-id phase1-fake-cell `
  --validation-cell-status warmup `
  --telemetry phase1-fake.jsonl `
  --manifest phase1-fake-manifest.json `
  --run-id phase1-fake-smoke
.\build\Debug\prism-validate-lifecycle.exe phase1-fake.jsonl
```

Result:

```text
status: ok
lifecycle valid: true
backend: fake
validation_cell_status: warmup
allocator_peak_bytes: 8
warmup_peak_bytes: 8
unknown_post_warmup_bytes: 0
host_telemetry_available: true
```

Llama mode without dependency pins fail-closed evidence:

```powershell
.\build\Debug\prism-probe.exe `
  --backend llama `
  --telemetry phase1-llama.jsonl `
  --manifest phase1-llama-manifest.json `
  --run-id phase1-llama-no-pins
.\build\Debug\prism-validate-lifecycle.exe phase1-llama.jsonl
```

Result:

```text
status: failed_closed
failure_reason: dependency_pins_required
lifecycle valid: true
backend: llama
```

## Local Artifact Hashes

The generated evidence files are intentionally ignored by `.gitignore`.

| Artifact | SHA-256 |
|---|---|
| `phase1-fake.jsonl` | `49E95368628E642973E3564426957C38E9D12B529F4C3595ED091DD250F2785E` |
| `phase1-fake-manifest.json` | `F07953A627A84EC5944B5E0BD21D627F5A19E49AC8162EF24609FED11F6F947B` |
| `phase1-llama.jsonl` | `FBB802BE90EBE90450AA9CEE3628922AA2022E4690852B813D3DBF3C098D2229` |
| `phase1-llama-manifest.json` | `4FCA2ED11163A188E7650B4E9D5A30BF7FD8487BFE70FAE3DC7732AD6C2C0CE0` |
| `phase1-llama-pinned.jsonl` | `2F5FC6721D435FD0A36DC7168C559126AE642B1EA7E39BE35BAD798918B46352` |
| `phase1-llama-pinned-manifest.json` | `1CBB6B0DEA6E600B77D4C5EAFE9EA2675916B6D7E20B4FDA4DEC8D9F8696AC3F` |

## Claim Status

Allowed current Phase 1 claims:

- PrismInfer enforces the current maximum GPU hard-cap policy in config parsing
  and schemas.
- PrismInfer emits validation-cell fields in telemetry and benchmark manifests.
- PrismInfer can run null/fake backend warmup paths through the backend adapter
  boundary.
- PrismInfer records backend-owned fake warmup memory in cap certification.
- PrismInfer records minimum host-memory and process-IO evidence on Windows.
- PrismInfer records the current llama.cpp/GGML/GGUF source pin for opt-in
  integration work.
- Llama mode fails closed without dependency pins.
- Opt-in compiled llama mode fails closed with `llama_backend_not_implemented`
  when dependency pins are supplied.

Disallowed current Phase 1 claims:

- PrismInfer fully governs llama.cpp or GGML allocations.
- PrismInfer has loaded a real GGUF through llama.cpp.
- PrismInfer has real `<=2B` or `>2B-5B` backend warmup evidence.
- PrismInfer can promote Phase 2 KV work from real backend evidence.
