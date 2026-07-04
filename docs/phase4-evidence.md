# Phase 4 Evidence

Phase 4 adds large-model and 90B claim validation gates. The purpose is to make
overclassified claims fail closed before they can be published.

## Implemented

- claim taxonomy enforcement for `metadata-only`, `simulated`,
  `measured-offload`, `validated-benchmark`, `deployable-profile`, and
  `rejected`,
- hybrid resident-GPU plan gate with the current 16 GiB maximum cap,
- `prism-plan-90b` simulated planner output that is never deployable,
- `prism-validate-claim` tool for deterministic claim classification,
- usability and repeatability threshold policy,
- evidence bundle validator and schema fixture,
- lifecycle v0.5 claim events:
  - `hybrid_plan_created`
  - `claim_classified`
  - `usability_result`
  - `repeatability_result`
  - `claim_validation_result`

## Evidence Boundary

Phase 4 does not prove deployable 90B inference. It proves that simulated,
incomplete, or overclassified large-model claims are rejected unless the exact
evidence bundle supports the requested label.

## Required Local Evidence

```text
phase4-90b-simulated-plan.json
phase4-overclaim-rejected.json
phase4-simulated-claim.jsonl
phase4-simulated-claim-manifest.json
```

## Local Evidence Hashes

| Artifact | SHA-256 |
|---|---|
| `phase4-90b-simulated-plan.json` | `3D440647CBB1ED773FDF79EE93E26D3EDC1275180E43F21E8E60572318A65DD2` |
| `phase4-overclaim-rejected.json` | `8FF8CDE87E2D2603EE2E0A0AD8A3875C8A33FECD3D8760DDFBC4B149A7472BD5` |
| `phase4-simulated-claim.jsonl` | `4831FA5A975059DBD870915004FBEC170D1EE8E858FEAF6EB707D48CE46CEEEA` |
| `phase4-simulated-claim-manifest.json` | `9789ACCC3DFCEA55E0795AD30D0B34E9A1F447BA1389954F4E7503025DC5C536` |

## Verification Commands

```powershell
cmake -S . -B build
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1
.\build\Debug\prism-plan-90b.exe --hard-cap-bytes 17179869184
.\build\Debug\prism-validate-claim.exe --claim-label deployable-profile --validation-cell-id simulated-90b-under-16gib --simulated-evidence
.\build\Debug\prism-validate-lifecycle.exe phase4-simulated-claim.jsonl
```
