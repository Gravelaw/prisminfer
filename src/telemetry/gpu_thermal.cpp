#include "prisminfer/gpu_thermal.h"

#include "prisminfer/telemetry.h"

#include <chrono>
#include <limits>

#if defined(PRISMINFER_ENABLE_CUDA_PROBE)
#include <nvml.h>
#endif

namespace prisminfer {

bool nvml_field_timestamps_are_fresh(
    std::uint64_t first_microseconds, std::uint64_t second_microseconds,
    std::uint64_t query_start_microseconds,
    std::uint64_t query_end_microseconds,
    std::uint64_t maximum_age_microseconds) {
  if (first_microseconds == 0U || first_microseconds != second_microseconds ||
      query_start_microseconds == 0U || query_end_microseconds < query_start_microseconds ||
      maximum_age_microseconds == 0U) return false;
  const auto earliest = query_start_microseconds > maximum_age_microseconds
                            ? query_start_microseconds - maximum_age_microseconds
                            : 0U;
  return first_microseconds >= earliest &&
         first_microseconds <= query_end_microseconds;
}

std::optional<GpuThermalSample> make_ada_tlimit_thermal_sample(
    std::uint32_t current_celsius, std::uint32_t target_celsius,
    std::int32_t raw_slowdown_tlimit,
    std::uint64_t captured_monotonic_milliseconds, bool thermal_throttling,
    bool power_brake_slowdown) {
  const auto slowdown_absolute = static_cast<std::int64_t>(target_celsius) -
                                 static_cast<std::int64_t>(raw_slowdown_tlimit);
  if (captured_monotonic_milliseconds == 0U || current_celsius > 200U ||
      target_celsius > 200U || slowdown_absolute < 0 ||
      slowdown_absolute > 200) {
    return std::nullopt;
  }
  GpuThermalSample sample;
  sample.available = true;
  sample.captured_monotonic_milliseconds = captured_monotonic_milliseconds;
  sample.current_celsius = static_cast<std::int32_t>(current_celsius);
  sample.reported_target_celsius = static_cast<std::int32_t>(target_celsius);
  sample.reported_slowdown_celsius =
      static_cast<std::int32_t>(slowdown_absolute);
  sample.thermal_throttling = thermal_throttling;
  sample.power_brake_slowdown = power_brake_slowdown;
  return sample;
}

GpuThermalSample sample_nvml_gpu_thermal(const std::string& gpu_uuid) {
  GpuThermalSample sample;
#if defined(PRISMINFER_ENABLE_CUDA_PROBE)
  if (nvmlInit_v2() != NVML_SUCCESS) return sample;
  nvmlDevice_t device{};
  const auto handle_status = gpu_uuid.empty()
                                 ? NVML_ERROR_INVALID_ARGUMENT
                                 : nvmlDeviceGetHandleByUUID(
                                       gpu_uuid.c_str(), &device);
  unsigned int temperature = 0U;
  nvmlFieldValue_t tlimits[2]{};
  tlimits[0].fieldId = NVML_FI_DEV_TEMPERATURE_GPU_MAX_TLIMIT;
  tlimits[1].fieldId = NVML_FI_DEV_TEMPERATURE_SLOWDOWN_TLIMIT;
  unsigned long long throttle_reasons = 0ULL;
  const auto temperature_status =
      handle_status == NVML_SUCCESS
          ? nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU,
                                     &temperature)
          : NVML_ERROR_UNKNOWN;
  const auto field_query_start = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  const auto field_status = handle_status == NVML_SUCCESS
                                ? nvmlDeviceGetFieldValues(
                                      device, 2, tlimits)
                                : NVML_ERROR_UNKNOWN;
  const auto field_query_end = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  const auto throttle_status =
      handle_status == NVML_SUCCESS
          ? nvmlDeviceGetCurrentClocksThrottleReasons(device,
                                                       &throttle_reasons)
          : NVML_ERROR_UNKNOWN;
  (void)nvmlShutdown();
  const auto captured = monotonic_time_milliseconds();
  if (temperature_status != NVML_SUCCESS || field_status != NVML_SUCCESS ||
      tlimits[0].nvmlReturn != NVML_SUCCESS ||
      tlimits[1].nvmlReturn != NVML_SUCCESS ||
      tlimits[0].valueType != NVML_VALUE_TYPE_UNSIGNED_INT ||
      tlimits[1].valueType != NVML_VALUE_TYPE_SIGNED_INT ||
      field_query_start <= 0 || field_query_end <= 0 ||
      !nvml_field_timestamps_are_fresh(
          static_cast<std::uint64_t>(tlimits[0].timestamp),
          static_cast<std::uint64_t>(tlimits[1].timestamp),
          static_cast<std::uint64_t>(field_query_start),
          static_cast<std::uint64_t>(field_query_end), 500'000U) ||
      throttle_status != NVML_SUCCESS ||
      captured == 0U) {
    return sample;
  }
  const auto converted = make_ada_tlimit_thermal_sample(
      temperature, tlimits[0].value.uiVal, tlimits[1].value.siVal, captured,
      (throttle_reasons & nvmlClocksThrottleReasonHwThermalSlowdown) != 0ULL,
      (throttle_reasons & nvmlClocksThrottleReasonHwPowerBrakeSlowdown) != 0ULL);
  if (converted) sample = *converted;
#else
  (void)gpu_uuid;
#endif
  return sample;
}

}  // namespace prisminfer
