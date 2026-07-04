#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace prisminfer::kernels {

struct Q4Block {
  float scale{1.0F};
  std::uint8_t packed_values{0};
};

struct Q4DecodeGemvResult {
  bool ok{false};
  std::string status{"rejected"};
  std::string reason;
  double max_abs_error{0.0};
};

std::vector<float> q4_decode_gemv_reference(
    std::span<const Q4Block> weights,
    std::span<const float> input,
    std::uint32_t rows,
    std::uint32_t cols);

Q4DecodeGemvResult compare_q4_decode_gemv_to_reference(
    std::span<const float> candidate,
    std::span<const float> reference,
    double tolerance);

}  // namespace prisminfer::kernels
