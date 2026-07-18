#include "prisminfer/gpu_thermal.h"

#include "prisminfer/telemetry.h"

#if defined(PRISMINFER_ENABLE_CUDA_PROBE)
#include <nvml.h>
#endif

namespace prisminfer {

GpuThermalSample sample_nvml_gpu_thermal(std::uint32_t device_index) {
  GpuThermalSample sample;
#if defined(PRISMINFER_ENABLE_CUDA_PROBE)
  if (nvmlInit_v2() != NVML_SUCCESS) return sample;
  nvmlDevice_t device{};
  const auto handle_status =
      nvmlDeviceGetHandleByIndex_v2(device_index, &device);
  unsigned int temperature = 0U;
  unsigned int slowdown = 0U;
  unsigned long long throttle_reasons = 0ULL;
  const auto temperature_status =
      handle_status == NVML_SUCCESS
          ? nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU,
                                     &temperature)
          : NVML_ERROR_UNKNOWN;
  const auto slowdown_status =
      handle_status == NVML_SUCCESS
          ? nvmlDeviceGetTemperatureThreshold(
                device, NVML_TEMPERATURE_THRESHOLD_SLOWDOWN, &slowdown)
          : NVML_ERROR_UNKNOWN;
  const auto throttle_status =
      handle_status == NVML_SUCCESS
          ? nvmlDeviceGetCurrentClocksThrottleReasons(device,
                                                       &throttle_reasons)
          : NVML_ERROR_UNKNOWN;
  (void)nvmlShutdown();
  const auto captured = monotonic_time_milliseconds();
  if (temperature_status != NVML_SUCCESS ||
      slowdown_status != NVML_SUCCESS || captured == 0U) {
    return sample;
  }
  sample.available = true;
  sample.captured_monotonic_milliseconds = captured;
  sample.current_celsius = static_cast<std::int32_t>(temperature);
  sample.reported_slowdown_celsius = static_cast<std::int32_t>(slowdown);
  if (throttle_status == NVML_SUCCESS) {
    sample.thermal_throttling =
        (throttle_reasons & nvmlClocksThrottleReasonHwThermalSlowdown) != 0ULL;
    sample.power_brake_slowdown =
        (throttle_reasons & nvmlClocksThrottleReasonHwPowerBrakeSlowdown) !=
        0ULL;
  }
#else
  (void)device_index;
#endif
  return sample;
}

}  // namespace prisminfer
