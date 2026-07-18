#include "prisminfer/gpu_thermal.h"

#include <cstdint>
#include <iostream>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

}  // namespace

int main() {
  const auto converted = prisminfer::make_ada_tlimit_thermal_sample(
      66U, 87U, -2, 100U, false, false);
  if (!expect(converted.has_value(), "signed T.Limit offset converts") ||
      !expect(converted->reported_slowdown_celsius == 89,
              "negative offset raises absolute slowdown bound") ||
      !expect(converted->reported_target_celsius == 87,
              "target remains retained")) {
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
                  1'000'000U, 1'000'000U, 1'100'000U, 1'100'100U, 500'000U),
              "recent matching fields accept") ||
      !expect(!prisminfer::nvml_field_timestamps_are_fresh(
                   1U, 1U, 1'100'000U, 1'100'100U, 500'000U),
              "stale fields reject") ||
      !expect(!prisminfer::nvml_field_timestamps_are_fresh(
                   1'700'000U, 1'700'000U, 1'100'000U, 1'100'100U, 500'000U),
              "future fields reject") ||
      !expect(!prisminfer::nvml_field_timestamps_are_fresh(
                   1'000'000U, 1'000'001U, 1'100'000U, 1'100'100U, 500'000U),
              "unequal fields reject")) {
    return 1;
  }
  return 0;
}
