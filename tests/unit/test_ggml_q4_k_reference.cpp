#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "prisminfer/kernels/ggml_q4_k_reference.h"

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
  prisminfer::kernels::GgmlQ4KBlock block{};
  block.delta_fp16 = 0x3C00U;    // 1.0
  block.minimum_fp16 = 0x3800U;  // 0.5
  block.scales = {2U, 3U, 4U, 5U, 1U, 2U, 3U, 4U, 0x56U, 0x78U, 0x9AU, 0xBCU};
  for (std::size_t index = 0U; index < block.quants.size(); ++index) {
    block.quants[index] = static_cast<std::uint8_t>(0xA0U | (index & 0x0FU));
  }

  const std::vector<prisminfer::kernels::GgmlQ4KBlock> blocks = {block};
  const auto decoded =
      prisminfer::kernels::decode_ggml_q4_k_reference(blocks, 1024U);
  if (expect(decoded.ok, "Q4_K block decodes")) return 1;
  if (expect(decoded.values.size() == 256U, "Q4_K has 256 values")) return 1;
  if (expect(std::fabs(decoded.values[0] + 0.5F) < 0.0001F,
             "first low-nibble value")) {
    return 1;
  }
  if (expect(std::fabs(decoded.values[32] - 29.0F) < 0.0001F,
             "first high-nibble value")) {
    return 1;
  }
  if (expect(std::fabs(decoded.values[128] + 2.5F) < 0.0001F,
             "packed scale subblock value")) {
    return 1;
  }
  if (expect(std::fabs(decoded.values[255] - 114.5F) < 0.0001F,
             "last packed scale subblock value")) {
    return 1;
  }
  const auto rejected =
      prisminfer::kernels::decode_ggml_q4_k_reference(blocks, 1023U);
  if (expect(!rejected.ok, "decoded byte limit rejects oversized output")) {
    return 1;
  }
  if (expect(rejected.reason == "decoded_byte_limit_exceeded",
             "decoded byte limit reason")) {
    return 1;
  }
  const std::size_t blocks_above_limit =
      std::vector<float>{}.max_size() / 256U + 1U;
  if (expect(blocks_above_limit <=
                 std::numeric_limits<std::size_t>::max() / 256U,
             "vector limit fixture avoids count overflow")) {
    return 1;
  }
  const auto limit_rejected =
      prisminfer::kernels::validate_ggml_q4_k_decode_limits(
          blocks_above_limit, std::numeric_limits<std::size_t>::max());
  if (expect(!limit_rejected.ok, "decoded value limit rejects before access")) {
    return 1;
  }
  if (expect(limit_rejected.reason == "decoded_value_limit_exceeded",
             "decoded value limit reason")) {
    return 1;
  }
  return 0;
}
