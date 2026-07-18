#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

#include "prisminfer/checked_arithmetic.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

}  // namespace

int main() {
  using prisminfer::RatioRounding;
  constexpr auto kU64Max = std::numeric_limits<std::uint64_t>::max();

  if (expect(prisminfer::checked_add_u64(7, 9) == 16,
             "checked add accepts an in-range sum") ||
      expect(prisminfer::checked_add_u64(kU64Max, 0) == kU64Max,
             "checked add accepts the exact maximum") ||
      expect(!prisminfer::checked_add_u64(kU64Max, 1),
             "checked add rejects overflow")) {
    return 1;
  }

  if (expect(prisminfer::checked_multiply_u64(7, 9) == 63,
             "checked multiply accepts an in-range product") ||
      expect(prisminfer::checked_multiply_u64(kU64Max, 1) == kU64Max,
             "checked multiply accepts the exact maximum") ||
      expect(prisminfer::checked_multiply_u64(kU64Max, 0) == 0,
             "checked multiply handles zero") ||
      expect(!prisminfer::checked_multiply_u64(kU64Max, 2),
             "checked multiply rejects overflow")) {
    return 1;
  }

  if (expect(prisminfer::checked_ceil_divide_u64(0, 7) == 0,
             "ceil divide preserves zero") ||
      expect(prisminfer::checked_ceil_divide_u64(8, 4) == 2,
             "ceil divide preserves an exact quotient") ||
      expect(prisminfer::checked_ceil_divide_u64(9, 4) == 3,
             "ceil divide rounds up") ||
      expect(prisminfer::checked_ceil_divide_u64(kU64Max, 1) == kU64Max,
             "ceil divide accepts the exact maximum") ||
      expect(!prisminfer::checked_ceil_divide_u64(1, 0),
             "ceil divide rejects a zero denominator")) {
    return 1;
  }

  const auto ten_percent_up =
      prisminfer::checked_basis_points_u64(10'001, 1'000, RatioRounding::Up);
  const auto ten_percent_down =
      prisminfer::checked_basis_points_u64(10'001, 1'000, RatioRounding::Down);
  const auto full_scale =
      prisminfer::checked_basis_points_u64(kU64Max, 10'000, RatioRounding::Up);
  if (expect(ten_percent_up == 1'001, "basis-point ratio rounds up exactly") ||
      expect(ten_percent_down == 1'000,
             "basis-point ratio rounds down exactly") ||
      expect(full_scale == kU64Max,
             "basis-point ratio accepts the full-scale maximum") ||
      expect(
          !prisminfer::checked_basis_points_u64(1, 10'001, RatioRounding::Up),
          "basis-point ratio rejects values above one hundred percent") ||
      expect(!prisminfer::checked_basis_points_u64(
                 1, 1, static_cast<RatioRounding>(99)),
             "basis-point ratio rejects an unknown rounding mode")) {
    return 1;
  }

  constexpr auto kU32Max = std::numeric_limits<std::uint32_t>::max();
  if (expect(prisminfer::checked_narrow_u32(kU32Max) == kU32Max,
             "u32 narrowing accepts the exact maximum") ||
      expect(!prisminfer::checked_narrow_u32(
                 static_cast<std::uint64_t>(kU32Max) + 1),
             "u32 narrowing rejects truncation") ||
      expect(
          prisminfer::checked_narrow_size(17) == static_cast<std::size_t>(17),
          "size narrowing accepts an in-range value")) {
    return 1;
  }

  if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
    if (expect(!prisminfer::checked_narrow_size(kU64Max),
               "size narrowing rejects truncation on a narrow platform")) {
      return 1;
    }
  } else if (expect(prisminfer::checked_narrow_size(kU64Max) ==
                        static_cast<std::size_t>(kU64Max),
                    "size narrowing accepts the u64 maximum on 64-bit")) {
    return 1;
  }

  return 0;
}
