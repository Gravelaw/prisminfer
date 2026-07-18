#include "prisminfer/gpu_thermal.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

}  // namespace

int main() {
  using Reason = prisminfer::GpuThermalUnavailableReason;
  constexpr std::array reason_names{
      std::pair{Reason::None, std::string_view{"none"}},
      std::pair{Reason::NvmlInitializationFailed,
                std::string_view{"nvml_initialization_failed"}},
      std::pair{Reason::GpuUuidInvalid, std::string_view{"gpu_uuid_invalid"}},
      std::pair{Reason::DeviceHandleFailed,
                std::string_view{"nvml_device_handle_failed"}},
      std::pair{Reason::TemperatureQueryFailed,
                std::string_view{"nvml_temperature_query_failed"}},
      std::pair{Reason::TlimitFieldQueryFailed,
                std::string_view{"nvml_tlimit_field_query_failed"}},
      std::pair{Reason::TargetTlimitFieldFailed,
                std::string_view{"nvml_target_tlimit_field_failed"}},
      std::pair{Reason::SlowdownTlimitFieldFailed,
                std::string_view{"nvml_slowdown_tlimit_field_failed"}},
      std::pair{Reason::TargetTlimitTypeInvalid,
                std::string_view{"nvml_target_tlimit_type_invalid"}},
      std::pair{Reason::SlowdownTlimitTypeInvalid,
                std::string_view{"nvml_slowdown_tlimit_type_invalid"}},
      std::pair{Reason::FieldQueryClockInvalid,
                std::string_view{"nvml_field_query_clock_invalid"}},
      std::pair{Reason::FieldTimestampInvalidOrStale,
                std::string_view{"nvml_field_timestamp_invalid_or_stale"}},
      std::pair{Reason::ClockEventQueryFailed,
                std::string_view{"nvml_clock_event_query_failed"}},
      std::pair{Reason::TemperatureValueInvalid,
                std::string_view{"nvml_temperature_value_invalid"}},
      std::pair{Reason::MonotonicClockInvalid,
                std::string_view{"monotonic_clock_invalid"}},
      std::pair{Reason::TlimitConversionInvalid,
                std::string_view{"nvml_tlimit_conversion_invalid"}},
      std::pair{Reason::NvmlShutdownFailed,
                std::string_view{"nvml_shutdown_failed"}},
      std::pair{Reason::CudaProbeDisabled,
                std::string_view{"cuda_probe_disabled"}},
  };
  for (const auto& [reason, expected_name] : reason_names) {
    if (!expect(prisminfer::gpu_thermal_unavailable_reason_name(reason) ==
                    expected_name,
                "typed thermal reason is stable")) {
      return 1;
    }
  }

  const auto converted = prisminfer::make_ada_tlimit_thermal_sample(
      66U, 87U, -2, 100U, false, false);
  if (!expect(converted.has_value(), "signed T.Limit offset converts") ||
      !expect(converted->reported_slowdown_celsius == 89,
              "negative offset raises absolute slowdown bound") ||
      !expect(converted->reported_target_celsius == 87,
              "target remains retained") ||
      !expect(converted->unavailable_reason == Reason::None,
              "available sample has no failure reason")) {
    return 1;
  }
  if (!expect(!prisminfer::make_ada_tlimit_thermal_sample(
                   66U, 87U, 0U, 0U, false, false),
              "zero timestamp rejects") ||
      !expect(!prisminfer::make_ada_tlimit_thermal_sample(
                   66U, 87U, 200U, 100U, false, false),
              "nonphysical absolute slowdown rejects") ||
      !expect(!prisminfer::make_ada_tlimit_thermal_sample(
                   201U, 87U, 0U, 100U, false, false),
              "invalid current temperature rejects")) {
    return 1;
  }
  if (!expect(prisminfer::nvml_field_timestamps_are_fresh(
                  1'000'000U, 1'000'001U, 1'100'000U, 1'100'100U, 500'000U),
              "recent bounded-skew fields accept") ||
      !expect(prisminfer::nvml_field_timestamps_are_fresh(
                  600'000U, 1'100'000U, 1'100'000U, 1'100'100U, 500'000U),
              "maximum bounded skew accepts") ||
      !expect(!prisminfer::nvml_field_timestamps_are_fresh(
                   1U, 1U, 1'100'000U, 1'100'100U, 500'000U),
              "stale fields reject") ||
      !expect(!prisminfer::nvml_field_timestamps_are_fresh(
                   1'100'000U, 1U, 1'100'000U, 1'100'100U, 500'000U),
              "stale second field rejects") ||
      !expect(!prisminfer::nvml_field_timestamps_are_fresh(
                   1'700'000U, 1'700'000U, 1'100'000U, 1'100'100U, 500'000U),
              "future fields reject") ||
      !expect(!prisminfer::nvml_field_timestamps_are_fresh(
                   1'100'101U, 1'100'101U, 1'100'000U, 1'100'100U, 500'000U),
              "future boundary rejects") ||
      !expect(!prisminfer::nvml_field_timestamps_are_fresh(
                   1'100'000U, 1'100'101U, 1'100'000U, 1'100'100U, 500'000U),
              "future second field rejects") ||
      !expect(!prisminfer::nvml_field_timestamps_are_fresh(
                   599'999U, 1'100'000U, 1'100'000U, 1'100'100U, 500'000U),
              "excessive skew rejects") ||
      !expect(!prisminfer::nvml_field_timestamps_are_fresh(
                   0U, 0U, 1'100'000U, 1'100'100U, 500'000U),
              "zero fields reject")) {
    return 1;
  }
  return 0;
}
