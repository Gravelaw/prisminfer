#pragma once

#include <cstdint>
#include <string>

#include "prisminfer/supervisor_admission.h"

namespace prisminfer {

// Samples the NVML device selected by the attended C2 preflight. The CUDA
// worker independently proves that its device LUID matches the DXGI adapter;
// unavailable or stale thermal evidence remains fail-closed.
[[nodiscard]] GpuThermalSample sample_nvml_gpu_thermal(
    const std::string& gpu_uuid);

}  // namespace prisminfer
