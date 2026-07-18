#include "prisminfer/gpu_thermal.h"

#include "prisminfer/telemetry.h"

#include <chrono>
#include <limits>

#if defined(PRISMINFER_ENABLE_CUDA_PROBE)
#include <nvml.h>
#endif

namespace prisminfer {

const char* gpu_thermal_unavailable_reason_name(
    GpuThermalUnavailableReason reason) noexcept {
  switch (reason) {
    case GpuThermalUnavailableReason::None:
      return "none";
    case GpuThermalUnavailableReason::NvmlInitializationFailed:
      return "nvml_initialization_failed";
    case GpuThermalUnavailableReason::GpuUuidInvalid:
      return "gpu_uuid_invalid";
    case GpuThermalUnavailableReason::DeviceHandleFailed:
      return "nvml_device_handle_failed";
    case GpuThermalUnavailableReason::TemperatureQueryFailed:
      return "nvml_temperature_query_failed";
    case GpuThermalUnavailableReason::TlimitFieldQueryFailed:
      return "nvml_tlimit_field_query_failed";
    case GpuThermalUnavailableReason::TargetTlimitFieldFailed:
      return "nvml_target_tlimit_field_failed";
    case GpuThermalUnavailableReason::SlowdownTlimitFieldFailed:
      return "nvml_slowdown_tlimit_field_failed";
    case GpuThermalUnavailableReason::TargetTlimitTypeInvalid:
      return "nvml_target_tlimit_type_invalid";
    case GpuThermalUnavailableReason::SlowdownTlimitTypeInvalid:
      return "nvml_slowdown_tlimit_type_invalid";
    case GpuThermalUnavailableReason::FieldQueryClockInvalid:
      return "nvml_field_query_clock_invalid";
    case GpuThermalUnavailableReason::FieldTimestampInvalidOrStale:
      return "nvml_field_timestamp_invalid_or_stale";
    case GpuThermalUnavailableReason::ClockEventQueryFailed:
      return "nvml_clock_event_query_failed";
    case GpuThermalUnavailableReason::TemperatureValueInvalid:
      return "nvml_temperature_value_invalid";
    case GpuThermalUnavailableReason::MonotonicClockInvalid:
      return "monotonic_clock_invalid";
    case GpuThermalUnavailableReason::TlimitConversionInvalid:
      return "nvml_tlimit_conversion_invalid";
    case GpuThermalUnavailableReason::NvmlShutdownFailed:
      return "nvml_shutdown_failed";
    case GpuThermalUnavailableReason::CudaProbeDisabled:
      return "cuda_probe_disabled";
  }
  return "unknown";
}

bool nvml_field_timestamps_are_fresh(
    std::uint64_t first_microseconds, std::uint64_t second_microseconds,
    std::uint64_t query_start_microseconds,
    std::uint64_t query_end_microseconds,
    std::uint64_t maximum_age_microseconds) {
  if (first_microseconds == 0U || second_microseconds == 0U ||
      query_start_microseconds == 0U || query_end_microseconds < query_start_microseconds ||
      maximum_age_microseconds == 0U) return false;
  const auto earliest = query_start_microseconds > maximum_age_microseconds
                            ? query_start_microseconds - maximum_age_microseconds
                            : 0U;
  return first_microseconds >= earliest && first_microseconds <= query_end_microseconds &&
         second_microseconds >= earliest && second_microseconds <= query_end_microseconds &&
         (first_microseconds >= second_microseconds
              ? first_microseconds - second_microseconds
              : second_microseconds - first_microseconds) <= maximum_age_microseconds;
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
  if (nvmlInit_v2() != NVML_SUCCESS) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::NvmlInitializationFailed;
    return sample;
  }
  nvmlDevice_t device{};
  const auto handle_status = gpu_uuid.empty()
                                 ? NVML_ERROR_INVALID_ARGUMENT
                                 : nvmlDeviceGetHandleByUUID(
                                       gpu_uuid.c_str(), &device);
  nvmlTemperature_t temperature_reading{};
  temperature_reading.version = nvmlTemperature_v1;
  temperature_reading.sensorType = NVML_TEMPERATURE_GPU;
  nvmlFieldValue_t tlimits[2]{};
  tlimits[0].fieldId = NVML_FI_DEV_TEMPERATURE_GPU_MAX_TLIMIT;
  tlimits[1].fieldId = NVML_FI_DEV_TEMPERATURE_SLOWDOWN_TLIMIT;
  unsigned long long throttle_reasons = 0ULL;
  const auto temperature_status =
      handle_status == NVML_SUCCESS
          ? nvmlDeviceGetTemperatureV(device, &temperature_reading)
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
          ? nvmlDeviceGetCurrentClocksEventReasons(device, &throttle_reasons)
          : NVML_ERROR_UNKNOWN;
  const auto shutdown_status = nvmlShutdown();
  const auto captured = monotonic_time_milliseconds();
  if (gpu_uuid.empty()) {
    sample.unavailable_reason = GpuThermalUnavailableReason::GpuUuidInvalid;
    return sample;
  }
  if (handle_status != NVML_SUCCESS) {
    sample.unavailable_reason = GpuThermalUnavailableReason::DeviceHandleFailed;
    return sample;
  }
  if (temperature_status != NVML_SUCCESS) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::TemperatureQueryFailed;
    return sample;
  }
  if (field_status != NVML_SUCCESS) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::TlimitFieldQueryFailed;
    return sample;
  }
  if (tlimits[0].nvmlReturn != NVML_SUCCESS) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::TargetTlimitFieldFailed;
    return sample;
  }
  if (tlimits[1].nvmlReturn != NVML_SUCCESS) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::SlowdownTlimitFieldFailed;
    return sample;
  }
  if (tlimits[0].valueType != NVML_VALUE_TYPE_UNSIGNED_INT) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::TargetTlimitTypeInvalid;
    return sample;
  }
  if (tlimits[1].valueType != NVML_VALUE_TYPE_SIGNED_INT) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::SlowdownTlimitTypeInvalid;
    return sample;
  }
  if (field_query_start <= 0 || field_query_end <= 0) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::FieldQueryClockInvalid;
    return sample;
  }
  if (!nvml_field_timestamps_are_fresh(
          static_cast<std::uint64_t>(tlimits[0].timestamp),
          static_cast<std::uint64_t>(tlimits[1].timestamp),
          static_cast<std::uint64_t>(field_query_start),
          static_cast<std::uint64_t>(field_query_end), 500'000U)) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::FieldTimestampInvalidOrStale;
    return sample;
  }
  if (throttle_status != NVML_SUCCESS) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::ClockEventQueryFailed;
    return sample;
  }
  if (temperature_reading.temperature < 0 ||
      temperature_reading.temperature > 200) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::TemperatureValueInvalid;
    return sample;
  }
  if (captured == 0U) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::MonotonicClockInvalid;
    return sample;
  }
  if (shutdown_status != NVML_SUCCESS) {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::NvmlShutdownFailed;
    return sample;
  }
  const auto converted = make_ada_tlimit_thermal_sample(
      static_cast<std::uint32_t>(temperature_reading.temperature),
      tlimits[0].value.uiVal, tlimits[1].value.siVal, captured,
      (throttle_reasons & nvmlClocksThrottleReasonHwThermalSlowdown) != 0ULL,
      (throttle_reasons & nvmlClocksThrottleReasonHwPowerBrakeSlowdown) != 0ULL);
  if (converted) {
    sample = *converted;
  } else {
    sample.unavailable_reason =
        GpuThermalUnavailableReason::TlimitConversionInvalid;
  }
#else
  (void)gpu_uuid;
  sample.unavailable_reason = GpuThermalUnavailableReason::CudaProbeDisabled;
#endif
  return sample;
}

}  // namespace prisminfer
