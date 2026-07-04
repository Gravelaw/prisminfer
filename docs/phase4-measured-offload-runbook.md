# Phase 4 Measured-Offload Runbook

Measured large-model evidence must use external model artifacts. Do not commit
weights, shards, prompt fixtures, or generated large telemetry bundles.

Required retained evidence:

- exact model id and model hash,
- quantization format and quant artifact hash,
- dependency pin file,
- telemetry JSONL,
- benchmark manifest,
- quality result,
- profitability result,
- usability and repeatability result,
- hardware profile and driver mode,
- host memory, IO, transfer, and NVMe counters where applicable.

Promotion rule:

- `measured-offload` is allowed for real bounded execution that is not fully
  validated.
- `validated-benchmark` requires cap, quality, profitability, usability, and
  repeatability gates.
- `deployable-profile` additionally requires an operating envelope and
  repeatable deployment procedure.

