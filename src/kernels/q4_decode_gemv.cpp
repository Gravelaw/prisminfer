#include "prisminfer/kernels/q4_decode_gemv.h"

#include <algorithm>
#include <cmath>

namespace prisminfer::kernels {

namespace {

float decode_nibble(std::uint8_t nibble, float scale) {
  const int signed_value = static_cast<int>(nibble & 0x0F) - 8;
  return static_cast<float>(signed_value) * scale;
}

}  // namespace

std::vector<float> q4_decode_gemv_reference(
    std::span<const Q4Block> weights,
    std::span<const float> input,
    std::uint32_t rows,
    std::uint32_t cols) {
  std::vector<float> output(rows, 0.0F);
  const std::uint64_t values_per_row = cols;
  const std::uint64_t blocks_per_row = (values_per_row + 1U) / 2U;
  for (std::uint32_t row = 0; row < rows; ++row) {
    float sum = 0.0F;
    for (std::uint32_t col = 0; col < cols; ++col) {
      const std::uint64_t block_index =
          static_cast<std::uint64_t>(row) * blocks_per_row + (col / 2U);
      if (block_index >= weights.size() || col >= input.size()) {
        continue;
      }
      const auto block = weights[block_index];
      const std::uint8_t nibble =
          (col % 2U == 0U) ? (block.packed_values & 0x0FU)
                           : ((block.packed_values >> 4U) & 0x0FU);
      sum += decode_nibble(nibble, block.scale) * input[col];
    }
    output[row] = sum;
  }
  return output;
}

Q4DecodeGemvResult compare_q4_decode_gemv_to_reference(
    std::span<const float> candidate,
    std::span<const float> reference,
    double tolerance) {
  if (candidate.size() != reference.size()) {
    return {false, "rejected", "output_size_mismatch", 0.0};
  }
  double max_abs_error = 0.0;
  for (std::size_t i = 0; i < candidate.size(); ++i) {
    max_abs_error =
        std::max(max_abs_error,
                 static_cast<double>(std::fabs(candidate[i] - reference[i])));
  }
  if (max_abs_error > tolerance) {
    return {false, "rejected", "correctness_tolerance_exceeded",
            max_abs_error};
  }
  return {true, "accepted", "", max_abs_error};
}

}  // namespace prisminfer::kernels
