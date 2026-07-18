#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace prisminfer {

enum class RatioRounding {
  Down,
  Up,
};

[[nodiscard]] std::optional<std::uint64_t> checked_add_u64(std::uint64_t left,
                                                           std::uint64_t right);
[[nodiscard]] std::optional<std::uint64_t> checked_multiply_u64(
    std::uint64_t left, std::uint64_t right);
[[nodiscard]] std::optional<std::uint64_t> checked_ceil_divide_u64(
    std::uint64_t numerator, std::uint64_t denominator);
[[nodiscard]] std::optional<std::uint64_t> checked_basis_points_u64(
    std::uint64_t value, std::uint32_t basis_points, RatioRounding rounding);
[[nodiscard]] std::optional<std::uint32_t> checked_narrow_u32(
    std::uint64_t value);
[[nodiscard]] std::optional<std::size_t> checked_narrow_size(
    std::uint64_t value);

}  // namespace prisminfer
