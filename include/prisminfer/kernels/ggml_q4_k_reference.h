#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace prisminfer::kernels {

// Exact block layout for GGML_TYPE_Q4_K at the pinned llama.cpp/GGML revision.
// This is a CPU reference only. It does not make Q4_K eligible for a particular
// artifact or CUDA path; #80 must supply the artifact inventory and eligibility.
struct GgmlQ4KBlock {
  std::uint16_t delta_fp16{0};
  std::uint16_t minimum_fp16{0};
  std::array<std::uint8_t, 12> scales{};
  std::array<std::uint8_t, 128> quants{};
};

static_assert(sizeof(GgmlQ4KBlock) == 144U,
              "GGML_TYPE_Q4_K block must remain 144 bytes");

struct GgmlQ4KDecodeResult {
  bool ok{false};
  std::string reason;
  std::vector<float> values;
};

// Decodes complete Q4_K blocks only. The returned values are CPU-reference
// data for fixture comparison and must not be treated as resident model weights.
// Callers must declare the maximum permitted decoded bytes before allocation.
GgmlQ4KDecodeResult decode_ggml_q4_k_reference(
    std::span<const GgmlQ4KBlock> blocks, std::size_t maximum_decoded_bytes);

}  // namespace prisminfer::kernels
