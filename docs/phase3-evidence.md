# Phase 3 Evidence

Phase 3 adds transfer-inclusive profitability accounting for offload claims.

## Implemented

- transfer metrics model for H2D, D2H, NVMe, wait, prefill, decode, TTFT, and
  token latency fields,
- profitability policy with `off`, `warn`, and `fail-closed` modes,
- offload planner contract for `none`, `gpu`, `host-kv`, `nvme-simulated`, and
  `nvme-experimental`,
- lifecycle v0.4 events:
  - `baseline_selected`
  - `transfer_plan`
  - `transfer_sample`
  - `offload_sample`
  - `profitability_result`
- benchmark manifest v0.4 transfer and profitability fields,
- deterministic accepted and rejected Phase 3 probe evidence.

## Evidence Boundary

Phase 3 closes the measurement and policy gate. It does not claim:

- optimized offload scheduling,
- GPUDirect Storage,
- unified-memory oversubscription governance,
- deployable 90B inference,
- profitability outside the exact validation cell.

## Required Local Evidence

The retained local evidence set should include:

```text
phase3-profitability-accepted.jsonl
phase3-profitability-accepted-manifest.json
phase3-profitability-rejected.jsonl
phase3-profitability-rejected-manifest.json
phase3-nvme-cold-cache-rejected.jsonl
phase3-nvme-cold-cache-rejected-manifest.json
```

## Local Evidence Hashes

| Artifact | SHA-256 |
|---|---|
| `phase3-profitability-accepted.jsonl` | `CB0417BA2F90E71FEAC4D0A981475269140B5D164891EE3B44C6DAEDB6E0D5F6` |
| `phase3-profitability-accepted-manifest.json` | `E236381954D668109A7840749F54CD5AD18F782609A506566D55AC706E6FA936` |
| `phase3-profitability-rejected.jsonl` | `BC1F33EED3B100AD6337EB48B09EEEAB5A2C81CC812C91B9A66D841B2904D469` |
| `phase3-profitability-rejected-manifest.json` | `DE0731FDD3493FE56233EF86B13ED16597A0615CD9C4E8491E2003145B379004` |
| `phase3-nvme-cold-cache-rejected.jsonl` | `2BAC63E28F3EEBBA312D7BC155AF832A47D561C2D44EFB44CD68CC45B0B8FE88` |
| `phase3-nvme-cold-cache-rejected-manifest.json` | `6C75100651117861B3CB7EFE1A03D9C48CD319C3B5231DCA57EF58DBA6910DD9` |

## Verification Commands

```powershell
cmake -S . -B build
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\prism-validate-lifecycle.exe phase3-profitability-accepted.jsonl
.\build\Debug\prism-validate-lifecycle.exe phase3-profitability-rejected.jsonl
.\build\Debug\prism-validate-lifecycle.exe phase3-nvme-cold-cache-rejected.jsonl
```
