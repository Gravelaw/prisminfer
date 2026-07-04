# Phase 1 Implementation Plan: Backend Adapter and Runtime Warmup Evidence

Phase 1 starts after Phase 0 telemetry/governance scaffolding. Its goal is to
prove PrismInfer can govern a real backend warmup path before it claims real
constrained-VRAM inference.

## Phase 1 Goal

Attach llama.cpp/GGML/GGUF through a narrow backend adapter boundary and replace
the placeholder `backend_warmup` with real backend allocation evidence.

## Non-Goals

- No custom CUDA kernels.
- No PrismInfer PagedAttention kernel.
- No KV quantization kernels.
- No GPUDirect Storage.
- No REST API.
- No 90B deployable claim.

## Architecture Direction

Do not embed llama.cpp directly into `tools/prism-probe/main.cpp`.

Before llama.cpp adapter work, Phase 1 must add validation-matrix scaffolding
and enforce the current maximum GPU cap:

```text
16 GiB = 17179869184 bytes
```

No Phase 1 benchmark claim should request or validate a GPU cap above this
ceiling.

Add a backend boundary first:

```cpp
struct BackendPlan {
  std::string backend_name;
  std::string backend_version;
  std::filesystem::path model_path;
  std::string model_parameter_bucket;
  std::uint64_t parameter_count;
  std::uint64_t model_size_bytes;
  std::uint64_t requested_context_tokens;
  std::uint64_t hard_vram_cap_bytes;
  std::uint32_t vram_tier_gib;
  std::uint32_t gpu_layers;
  bool mmap_enabled;
  bool gpu_requested;
};

struct BackendWarmupResult {
  bool ok;
  std::string failure_reason;
  MemorySample memory_sample;
  std::uint64_t backend_owned_peak_bytes;
  std::uint64_t backend_external_peak_bytes;
  std::uint64_t retained_pool_bytes;
};

class BackendAdapter {
 public:
  virtual BackendWarmupResult warmup(const RuntimeConfig& config,
                                     CappedAllocatorTracker& allocator) = 0;
};
```

Add a memory observer boundary before real backend certification:

```cpp
struct BackendMemoryObservation {
  std::uint64_t prisminfer_owned_bytes;
  std::uint64_t backend_owned_bytes;
  std::uint64_t cuda_context_bytes;
  std::uint64_t kv_estimated_bytes;
  std::uint64_t workspace_estimated_bytes;
  std::uint64_t retained_pool_bytes;
  std::uint64_t unknown_bytes;
  std::string evidence_status; // observed | reconciled | unknown
};
```

Any unexplained backend growth above tolerance remains fail-closed. Until
llama.cpp/GGML allocation paths are bridged or reconciled, use the phrase
`observed backend allocation evidence`, not `full governed backend allocation`.

Initial adapters:

- `NullBackendAdapter`: preserves current Phase 0 behavior.
- `FakeBackendAdapter`: deterministic test harness for success and failure.
- `LlamaBackendAdapter`: opt-in build after pins and schema gates exist.

## Lifecycle v0.2 Target

Current Phase 0 lifecycle remains valid for probe-only modes. Phase 1 adds:

```text
backend_selected
dependency_pins_resolved
model_load_plan
backend_init
backend_warmup
```

Target order:

```text
run_start
config_validated
telemetry_probe
model_sidecar_validated
backend_selected
dependency_pins_resolved
cuda_context_probe, if GPU requested
cap_semantics_resolved
host_prepare
model_load_plan
backend_init
backend_warmup
memory_sample
cap_certification_result
completed | failed_closed
run_end
```

## Config and CLI Additions

Add config fields:

```json
{
  "backend": "null|fake|llama",
  "backend_required": false,
  "model_parameter_bucket": "",
  "parameter_count": 0,
  "vram_tier_gib": 1,
  "validation_cell_id": "",
  "validation_cell_status": "not-started|metadata-only|warmup|decode-smoke|rejected",
  "dependency_pin_file": "",
  "context_tokens": 512,
  "gpu_layers": 0,
  "mmap": true,
  "warmup_tokens": 1,
  "benchmark_hard_cap": true
}
```

Add CLI flags:

```text
--backend null|fake|llama
--model-parameter-bucket BUCKET
--parameter-count N
--vram-tier-gib N
--validation-cell-id ID
--validation-cell-status STATUS
--dependency-pin-file PATH
--context-tokens N
--gpu-layers N
--mmap on|off
--warmup-tokens N
```

## Manifest v0.2 Additions

Record:

- backend name and version,
- backend adapter contract version,
- validation cell id and status,
- model parameter bucket,
- parameter count,
- VRAM tier,
- hard VRAM cap bytes,
- llama.cpp commit,
- GGML commit or source record,
- GGUF schema/source record,
- model path, size, and hash,
- sidecar path and hash,
- context tokens,
- GPU layers,
- mmap setting,
- warmup tokens,
- backend owned peak bytes,
- backend external peak bytes,
- CUDA context/runtime bytes,
- minimal KV estimated bytes and evidence status,
- workspace estimated bytes,
- unknown backend bytes,
- retained pool bytes,
- host working set/RSS equivalent,
- private bytes,
- commit/pagefile pressure where available,
- process IO read/write bytes,
- cold-cache or warm-cache marker,
- Windows driver model/TDR assumptions where available,
- cap certification status,
- failure reason.

## File and Module Candidates

New files:

```text
include/prisminfer/backend.h
include/prisminfer/backend_memory_observer.h
include/prisminfer/gpu_cap_policy.h
include/prisminfer/host_memory_tracker.h
include/prisminfer/kv_model_metadata.h
include/prisminfer/memory_ledger.h
src/backend/null_backend.cpp
src/backend/fake_backend.cpp
src/backend/llama_backend.cpp
src/governor/gpu_cap_policy.cpp
src/governor/memory_ledger.cpp
tests/unit/test_backend_adapter.cpp
tests/unit/test_backend_lifecycle.cpp
docs/dependency-pins.md
```

Likely modified files:

```text
CMakeLists.txt
include/prisminfer/config.h
include/prisminfer/runtime_state.h
include/prisminfer/benchmark.h
include/prisminfer/telemetry.h
src/config.cpp
src/benchmark/manifest_writer.cpp
src/telemetry/telemetry_jsonl.cpp
tools/prism-probe/main.cpp
tools/prism-validate-lifecycle/main.cpp
schemas/telemetry.schema.json
schemas/benchmark_manifest.schema.json
docs/runtime-state-machine.md
docs/cap-semantics.md
docs/risk-register.md
```

## Issue Slices

Create a Phase 1 milestone:

```text
Phase 1: Backend Adapter and Runtime Warmup Evidence
```

Suggested issues:

1. Enforce maximum VRAM cap policy and validation-matrix fields.
2. Create Phase 1 milestone, labels, and project fields.
3. Define backend adapter contract and null backend.
4. Add backend memory observer and memory ledger contracts.
5. Add backend config and CLI contract.
6. Extend telemetry lifecycle to v0.2.
7. Extend benchmark manifest to v0.2.
8. Add dependency pin record and integration decision for llama.cpp/GGML/GGUF.
9. Add fake backend contract test harness.
10. Refactor `prism-probe` orchestration around backend adapter.
11. Introduce llama.cpp adapter behind opt-in build flag.
12. Capture first real backend warmup evidence on a small pinned GGUF model.
13. Update Phase 1 risk register and exit audit.

## Test Gates

Hosted CI:

- Default build.
- Null backend tests.
- Fake backend tests.
- Config parser tests.
- Lifecycle validator v0.2 tests.
- Manifest writer v0.2 tests.
- Maximum cap policy tests.
- Validation cell field tests.
- Host-memory field availability tests.

Self-hosted CUDA CI:

- CUDA probe build.
- Null/fake backend GPU-probed smoke.
- Forced breach matrix.
- Later opt-in llama.cpp warmup evidence.

Local evidence:

- `scripts\verify.ps1`
- `scripts\verify.ps1 -WithCuda`
- Real model smoke only when dependency pins and artifacts are available.

## Implementation Order

1. Planning and GitHub tracking for Phase 1.
2. Maximum VRAM cap policy plus validation-matrix config/schema fields.
3. Backend adapter interface plus null backend.
4. Backend memory observer and memory ledger contracts.
5. Fake backend test harness.
6. Config and CLI additions.
7. Lifecycle v0.2.
8. Manifest v0.2.
9. Probe orchestration refactor.
10. Dependency pin record and integration-mode decision.
11. llama.cpp adapter behind opt-in build.
12. First real backend warmup evidence for `<=2B` and `>2B-5B` representative cells.
13. Phase 1 exit audit.

## Exit Criteria

Phase 1 exits only when:

- Real backend mode cannot run without dependency pins.
- No run can promote a GPU cap above 16 GiB.
- Representative `<=2B` and `>2B-5B` cells have retained warmup or rejection
  evidence before larger cells are claimed.
- A pinned llama.cpp/GGUF warmup emits lifecycle-valid telemetry.
- Manifest and telemetry agree on run id, backend, model hash, status, and cap
  result.
- Backend warmup peak participates in cap certification.
- Hidden or unknown backend allocations fail closed.
- Host memory and IO telemetry are present or explicitly unavailable with
  reasons.
- Hosted CI remains green for null/fake backends.
- Self-hosted CUDA CI remains green for CUDA-probed evidence paths.
