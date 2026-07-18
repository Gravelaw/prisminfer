#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "prisminfer/supervisor_admission.h"

namespace prisminfer {

// Ada-and-later NVML T.Limit offsets are signed relative to the reported GPU
// target temperature. Invalid or nonphysical conversions stay unavailable.
[[nodiscard]] std::optional<GpuThermalSample>
make_ada_tlimit_thermal_sample(std::uint32_t current_celsius,
                               std::uint32_t target_celsius,
                               std::int32_t raw_slowdown_tlimit,
                               std::uint64_t captured_monotonic_milliseconds,
                               bool thermal_throttling,
                               bool power_brake_slowdown);

// Each NVML field is independently timestamped. Both fields must be fresh and
// their bounded skew must not exceed the same maximum-age window.
[[nodiscard]] bool nvml_field_timestamps_are_fresh(
    std::uint64_t first_microseconds, std::uint64_t second_microseconds,
    std::uint64_t query_start_microseconds,
    std::uint64_t query_end_microseconds,
    std::uint64_t maximum_age_microseconds);

// Samples the NVML device selected by the attended C2 preflight. The CUDA
// worker independently proves that its device LUID matches the DXGI adapter;
// unavailable or stale thermal evidence remains fail-closed.
[[nodiscard]] GpuThermalSample sample_nvml_gpu_thermal(
    const std::string& gpu_uuid);

}  // namespace prisminfer
