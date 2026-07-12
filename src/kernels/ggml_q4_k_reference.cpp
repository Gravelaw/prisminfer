#include "prisminfer/kernels/ggml_q4_k_reference.h"

#include <bit>
#include <limits>
#include <new>

namespace prisminfer::kernels {

namespace {

constexpr std::size_t kValuesPerBlock = 256U;
constexpr std::size_t kSubblocksPerBlock = 8U;
constexpr std::size_t kValuesPerSubblock = 32U;

float fp16_to_float(std::uint16_t bits) {
  const std::uint32_t sign = static_cast<std::uint32_t>(bits & 0x8000U) << 16U;
  std::uint32_t exponent = (bits >> 10U) & 0x1FU;
  std::uint32_t mantissa = bits & 0x03FFU;
  std::uint32_t output = 0U;

  if (exponent == 0U) {
    if (mantissa == 0U) {
      output = sign;
    } else {
      exponent = 113U;
      while ((mantissa & 0x0400U) == 0U) {
        mantissa <<= 1U;
        --exponent;
      }
      mantissa &= 0x03FFU;
      output = sign | (exponent << 23U) | (mantissa << 13U);
    }
  } else if (exponent == 31U) {
    output = sign | 0x7F800000U | (mantissa << 13U);
  } else {
    output = sign | ((exponent + 112U) << 23U) | (mantissa << 13U);
  }
  return std::bit_cast<float>(output);
}

void decode_scale_and_minimum(const std::array<std::uint8_t, 12>& scales,
                              std::size_t subblock, std::uint8_t* scale,
                              std::uint8_t* minimum) {
  if (subblock < 4U) {
    *scale = scales[subblock] & 0x3FU;
    *minimum = scales[subblock + 4U] & 0x3FU;
    return;
  }
  *scale = static_cast<std::uint8_t>(
      (scales[subblock + 4U] & 0x0FU) |
      ((scales[subblock - 4U] >> 6U) << 4U));
  *minimum = static_cast<std::uint8_t>(
      (scales[subblock + 4U] >> 4U) |
      ((scales[subblock] >> 6U) << 4U));
}

}  // namespace

GgmlQ4KDecodeResult decode_ggml_q4_k_reference(
    std::span<const GgmlQ4KBlock> blocks, std::size_t maximum_decoded_bytes) {
  if (blocks.size() > std::numeric_limits<std::size_t>::max() /
                          kValuesPerBlock) {
    return {false, "decoded_value_count_overflow", {}};
  }
  const std::size_t decoded_values = blocks.size() * kValuesPerBlock;
  std::vector<float> values;
  if (decoded_values > values.max_size()) {
    return {false, "decoded_value_limit_exceeded", {}};
  }
  if (decoded_values > std::numeric_limits<std::size_t>::max() /
                           sizeof(float)) {
    return {false, "decoded_byte_count_overflow", {}};
  }
  const std::size_t decoded_bytes = decoded_values * sizeof(float);
  if (decoded_bytes > maximum_decoded_bytes) {
    return {false, "decoded_byte_limit_exceeded", {}};
  }

  try {
    values.reserve(decoded_values);
    for (const GgmlQ4KBlock& block : blocks) {
      const float delta = fp16_to_float(block.delta_fp16);
      const float minimum = fp16_to_float(block.minimum_fp16);
      for (std::size_t subblock = 0U; subblock < kSubblocksPerBlock;
           ++subblock) {
        std::uint8_t scale = 0U;
        std::uint8_t minimum_scale = 0U;
        decode_scale_and_minimum(block.scales, subblock, &scale, &minimum_scale);
        const float scaled_delta = delta * static_cast<float>(scale);
        const float scaled_minimum =
            minimum * static_cast<float>(minimum_scale);
        const std::size_t quant_offset = (subblock / 2U) * kValuesPerSubblock;
        const bool high_nibble = (subblock % 2U) != 0U;
        for (std::size_t value = 0U; value < kValuesPerSubblock; ++value) {
          const std::uint8_t packed = block.quants[quant_offset + value];
          const std::uint8_t quant = high_nibble ? (packed >> 4U)
                                                  : (packed & 0x0FU);
          values.push_back(scaled_delta * static_cast<float>(quant) -
                           scaled_minimum);
        }
      }
    }
    return {true, "", std::move(values)};
  } catch (const std::bad_alloc&) {
    return {false, "decoded_allocation_failed", {}};
  }
}

}  // namespace prisminfer::kernels
