# Phase 1 Exit Audit

This audit closes Phase 1 as backend evidence complete but not allocation
governance certified. Larger-phase work may use this evidence only as
fail-closed backend loading proof, not as proof that llama.cpp allocations are
fully governed by PrismInfer.

## Audit Result

Status: Pass with classified residual risk.

Phase 1 has retained lifecycle-valid artifacts for both required representative
cells:

| Cell | Artifact | Result |
|---|---|---|
| `<=2B @ 1 GiB` | `phase1-real-llama-15m-q2.jsonl`, `phase1-real-llama-15m-q2-manifest.json` | Real llama.cpp warmup completed; cap certification failed closed with `unknown_post_warmup_allocation`. |
| `>2B-5B @ 1 GiB` | `phase1-real-llama-qwen25-3b-q2.jsonl`, `phase1-real-llama-qwen25-3b-q2-manifest.json` | Real llama.cpp warmup completed; cap certification failed closed with `unknown_post_warmup_allocation`. |

Both cells retain model hash, sidecar validation, dependency pin, backend
selection, lifecycle validation, manifest/telemetry agreement, host memory, and
process IO evidence.

## Exit Criteria

| Criterion | Status | Evidence |
|---|---|---|
| Real backend mode cannot run without dependency pins. | Pass | `phase1-llama.jsonl` fails closed with `dependency_pins_required`. |
| No run can promote a GPU cap above 16 GiB. | Pass | Config/schema tests and `scripts\verify.ps1` cap policy checks pass. |
| Representative `<=2B` and `>2B-5B` cells have retained warmup or rejection evidence. | Pass | Both real llama.cpp cells have lifecycle-valid fail-closed artifacts in `docs/phase1-evidence.md`. |
| Pinned llama.cpp/GGUF warmup emits lifecycle-valid telemetry. | Pass | Both real cells emit `backend_warmup`, `cap_certification_result`, and final fail-closed events. |
| Manifest and telemetry agree on run id, backend, model hash, status, and cap result. | Pass | `prism-validate-lifecycle` accepts both JSONL files; manifests record matching status and failure reason. |
| Backend warmup peak participates in cap certification. | Pass | External backend bytes become `unknown_post_warmup_bytes`, causing fail-closed certification. |
| Hidden or unknown backend allocations fail closed. | Pass | Both real cells fail with `unknown_post_warmup_allocation`. |
| Host memory and IO telemetry are present or explicitly unavailable. | Pass | Both manifests record host telemetry and process IO fields. |
| Hosted CI remains green for null/fake backends. | Pass | PR #55 passed Ubuntu and Windows Debug/Release checks. |
| Self-hosted CUDA CI remains green for CUDA-probed evidence paths. | Pass | PR #55 passed Windows NVIDIA CUDA probe. |

## Residual Risk

The remaining risk is classified, not resolved: PrismInfer can observe
llama.cpp external allocation evidence and fail closed, but it cannot yet
certify those allocations as governed memory. Phase 2 must not claim real
backend allocation governance or promote KV work from certified real-backend
evidence until llama.cpp allocation paths are reconciled.

## Verification

Local verification for the Phase 1 closure set:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1
cmake -S . -B build -DPRISMINFER_ENABLE_LLAMA_BACKEND=ON
cmake --build build --config Debug --target prism-probe prism-validate-lifecycle test_backend_adapter
ctest --test-dir build -C Debug -R test_backend_adapter --output-on-failure
.\build\Debug\prism-validate-lifecycle.exe phase1-real-llama-15m-q2.jsonl
.\build\Debug\prism-validate-lifecycle.exe phase1-real-llama-qwen25-3b-q2.jsonl
cmake -S . -B build -DPRISMINFER_ENABLE_LLAMA_BACKEND=OFF
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1
```
