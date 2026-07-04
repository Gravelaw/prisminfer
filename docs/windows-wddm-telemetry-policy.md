# Windows WDDM Telemetry Policy

Windows/NVIDIA evidence is useful, but WDDM telemetry can be noisy and driver
mode can affect long-running behavior. PrismInfer must record the driver and
telemetry context for every CUDA-backed benchmark artifact.

## Required Fields

Record where available:

- GPU name and device id,
- total VRAM,
- NVIDIA driver version,
- CUDA driver version,
- CUDA runtime version,
- driver model: `WDDM`, `TCC`, or `unknown`,
- TDR enabled/assumed status,
- hardware scheduling mode where available,
- telemetry sampling cadence,
- whether process GPU memory telemetry was available,
- whether NVML process telemetry was available,
- self-hosted runner labels for CI evidence.

## Certification Rules

- Device-level memory delta can corroborate a run, but it cannot certify a hard
  cap by itself.
- Process-visible GPU peaks are preferred when available.
- Contradictory process/device evidence fails closed in benchmark mode.
- Missing driver-mode evidence prevents long-running offload claims from being
  promoted to validated status.

## Phase Ownership

- Phase 0: CUDA context and device telemetry smoke evidence.
- Phase 1: record driver/runtime fields for real backend warmup.
- Phase 3: require driver mode and TDR assumptions for offload profitability.
- Phase 4: include driver mode in all validated large-model evidence bundles.

