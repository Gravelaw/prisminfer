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
- opt-in process-backed llama.cpp adapter that can load and warm a pinned GGUF
  through an external llama.cpp CLI.

Phase 1 does not yet provide:

- reconciled llama.cpp allocation governance,
- representative `>2B-5B` real backend warmup or rejection artifacts,
- deployable or fully governed backend-allocation claims.

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

Opt-in llama mode with the pinned dependency file and no executable fails closed:

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
failure_reason: llama_executable_required
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

Real pinned llama.cpp `<=2B` warmup rejection evidence:

External artifacts:

- llama.cpp source commit: `ef2d770117db45b05aa7ecd1b0acca36370c5470`
- local CPU build wrapper:
  `D:\Research\tools\llama.cpp\ef2d770117db45b05aa7ecd1b0acca36370c5470\llama-cli-wrapper.cmd`
- model: `tensorblock/tinyllama-15M-stories-GGUF`,
  `tinyllama-15M-stories-Q2_K.gguf`
- model SHA-256:
  `F7E39DC9F26F3D39BF59E885349C6EEC65880F685322D591F53E6CDB46CEB2E9`

```powershell
.\build\Debug\prism-probe.exe `
  --backend llama `
  --backend-required `
  --dependency-pin-file third_party\llama.cpp-pin.json `
  --llama-executable D:\Research\tools\llama.cpp\ef2d770117db45b05aa7ecd1b0acca36370c5470\llama-cli-wrapper.cmd `
  --model D:\Research\models\tensorblock-tinyllama-15m-stories-q2_k\tinyllama-15M-stories-Q2_K.gguf `
  --sidecar D:\Research\models\tensorblock-tinyllama-15m-stories-q2_k\tinyllama-15M-stories-Q2_K.gguf.prism.json `
  --max-model-bytes 50000000 `
  --model-parameter-bucket '<=2B' `
  --parameter-count 15000000 `
  --vram-tier-gib 1 `
  --validation-cell-id phase1-real-llama-15m-q2-cpu `
  --validation-cell-status warmup `
  --context-tokens 64 `
  --warmup-tokens 1 `
  --gpu-layers 0 `
  --telemetry phase1-real-llama-15m-q2.jsonl `
  --manifest phase1-real-llama-15m-q2-manifest.json `
  --run-id phase1-real-llama-15m-q2
.\build\Debug\prism-validate-lifecycle.exe phase1-real-llama-15m-q2.jsonl
```

Result:

```text
backend_warmup status: ok
backend_external_peak_bytes: 26413622
evidence_status: unknown
cap_certification_result: certified=false
failure_reason: unknown_post_warmup_allocation
lifecycle valid: true
```

## Local Artifact Hashes

The generated evidence files are intentionally ignored by `.gitignore`.

| Artifact | SHA-256 |
|---|---|
| `phase1-fake.jsonl` | `40F5A4B843900758C48F3503C7F62592869DF088506099D987C33EC7FDF38851` |
| `phase1-fake-manifest.json` | `C9CFB1219C2BF817961BE3C47AEEF0AC1257268ECD5A72CFEF1F09A71866D382` |
| `phase1-llama.jsonl` | `DCA4F702E7E3F1C07F07857B980103CBE823068F2BBED8FF82A2AF7BF43A5103` |
| `phase1-llama-manifest.json` | `462A4F9B453E87E7AF8DD1C3C39CF29D1F0D44AC0DAAE7BEE452C51F99BB94CE` |
| `phase1-llama-pinned.jsonl` | `E7CD9FC35014196ECF59ED44D4928B03D9FFAE43FEEE69087375C60B365AAFBA` |
| `phase1-llama-pinned-manifest.json` | `09420CF35B214F32E907C1E91AD2FB5396D83B33383FE2BD4AD34B158A7EE96B` |
| `phase1-real-llama-15m-q2.jsonl` | `714DC474677944F468D60B245DE64DB01922267FB1534C9D7D59245B392211BE` |
| `phase1-real-llama-15m-q2-manifest.json` | `091EC86DE7912C78C6E16D3D82BDAD0C809432FD0A13F96C2BF917985A556C61` |

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
- Opt-in compiled llama mode can execute an external pinned llama.cpp CLI and
  record real GGUF warmup allocation evidence.
- Real llama.cpp `<=2B` evidence currently fails closed because external backend
  allocations are observed but not reconciled as governed memory.

Disallowed current Phase 1 claims:

- PrismInfer fully governs llama.cpp or GGML allocations.
- PrismInfer has certified real llama.cpp allocation governance.
- PrismInfer has real `>2B-5B` backend warmup or rejection evidence.
- PrismInfer can promote Phase 2 KV work from real backend evidence.
