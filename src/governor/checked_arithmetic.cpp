#include "prisminfer/checked_arithmetic.h"

#include <limits>

namespace prisminfer {

namespace {

constexpr std::uint64_t kBasisPointDenominator = 10'000;

}  // namespace

std::optional<std::uint64_t> checked_add_u64(std::uint64_t left,
                                             std::uint64_t right) {
  if (left > std::numeric_limits<std::uint64_t>::max() - right) {
    return std::nullopt;
  }
  return left + right;
}

std::optional<std::uint64_t> checked_multiply_u64(std::uint64_t left,
                                                  std::uint64_t right) {
  if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
    return std::nullopt;
  }
  return left * right;
}

std::optional<std::uint64_t> checked_ceil_divide_u64(
    std::uint64_t numerator, std::uint64_t denominator) {
  if (denominator == 0) {
    return std::nullopt;
  }
  const auto quotient = numerator / denominator;
  if (numerator % denominator == 0) {
    return quotient;
  }
  return checked_add_u64(quotient, 1);
}

std::optional<std::uint64_t> checked_basis_points_u64(
    std::uint64_t value, std::uint32_t basis_points, RatioRounding rounding) {
  if ((rounding != RatioRounding::Down && rounding != RatioRounding::Up) ||
      basis_points > kBasisPointDenominator) {
    return std::nullopt;
  }

  const auto quotient = value / kBasisPointDenominator;
  const auto remainder = value % kBasisPointDenominator;
  const auto base = checked_multiply_u64(quotient, basis_points);
  const auto remainder_product = checked_multiply_u64(remainder, basis_points);
  if (!base || !remainder_product) {
    return std::nullopt;
  }

  const auto remainder_quotient = *remainder_product / kBasisPointDenominator;
  const bool round_up = rounding == RatioRounding::Up &&
                        *remainder_product % kBasisPointDenominator != 0;
  const auto extra = checked_add_u64(remainder_quotient, round_up ? 1 : 0);
  if (!extra) {
    return std::nullopt;
  }
  return checked_add_u64(*base, *extra);
}

std::optional<std::uint32_t> checked_narrow_u32(std::uint64_t value) {
  if (value > std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(value);
}

std::optional<std::size_t> checked_narrow_size(std::uint64_t value) {
  if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
    if (value > std::numeric_limits<std::size_t>::max()) {
      return std::nullopt;
    }
  }
  return static_cast<std::size_t>(value);
}

}  // namespace prisminfer
