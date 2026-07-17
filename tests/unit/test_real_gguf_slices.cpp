#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "prisminfer/kernels/ggml_q4_k_reference.h"

namespace {
std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) throw std::runtime_error("open failed: " + path.string());
  const auto size = input.tellg();
  input.seekg(0);
  std::vector<std::byte> result(static_cast<std::size_t>(size));
  input.read(reinterpret_cast<char*>(result.data()), size);
  if (!input) throw std::runtime_error("read failed: " + path.string());
  return result;
}

template <typename Block, typename Decoder>
void compare_payload(const std::vector<std::byte>& raw,
                     const std::vector<std::byte>& expected_bytes,
                     const std::string& prefix, Decoder decoder) {
  if (raw.size() != sizeof(Block) || expected_bytes.size() != 256U * sizeof(float)) {
    throw std::runtime_error("retained slice size mismatch: " + prefix);
  }
  Block block{};
  std::memcpy(&block, raw.data(), sizeof(block));
  const auto actual = decoder(std::span<const Block>(&block, 1U), 1024U);
  if (!actual.ok || actual.values.size() != 256U) {
    throw std::runtime_error("reference decoder rejected: " + prefix);
  }
  for (std::size_t index = 0; index < actual.values.size(); ++index) {
    float expected = 0.0F;
    std::memcpy(&expected, expected_bytes.data() + index * sizeof(float), sizeof(float));
    if (!std::isfinite(expected) || !std::isfinite(actual.values[index])) {
      throw std::runtime_error("non-finite differential value: " + prefix);
    }
    const float tolerance = 1e-6F * std::max(1.0F, std::fabs(expected));
    if (std::fabs(actual.values[index] - expected) > tolerance) {
      throw std::runtime_error("upstream differential mismatch: " + prefix +
                               ":" + std::to_string(index));
    }
  }
}

std::vector<std::byte> from_hex(const std::string& value) {
  if (value.size() % 2U) throw std::runtime_error("odd hex payload");
  std::vector<std::byte> result(value.size() / 2U);
  for (std::size_t index = 0; index < result.size(); ++index) {
    const auto nibble = [](char c) -> unsigned {
      if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
      if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
      throw std::runtime_error("invalid hex payload");
    };
    result[index] = static_cast<std::byte>((nibble(value[index * 2U]) << 4U) |
                                           nibble(value[index * 2U + 1U]));
  }
  return result;
}

template <typename Block, typename Decoder>
void compare_slice(const std::filesystem::path& directory,
                   const std::string& prefix, Decoder decoder) {
  compare_payload<Block>(read_bytes(directory / (prefix + ".bin")),
                         read_bytes(directory / (prefix + "-f32.bin")),
                         prefix, decoder);
}
}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "--identity") {
    std::cout << "packet-a-real-slice-decoder-v1:"
              << PRISMINFER_REAL_SLICE_DECODER_ID << "\n";
    return 0;
  }
  if (argc == 4) {
    try {
      const auto raw = from_hex(argv[2]);
      const auto reference = from_hex(argv[3]);
      if (std::string(argv[1]) == "Q4_K") {
        compare_payload<prisminfer::kernels::GgmlQ4KBlock>(raw, reference, "held-Q4_K",
            prisminfer::kernels::decode_ggml_q4_k_reference);
      } else if (std::string(argv[1]) == "Q6_K") {
        compare_payload<prisminfer::kernels::GgmlQ6KBlock>(raw, reference, "held-Q6_K",
            prisminfer::kernels::decode_ggml_q6_k_reference);
      } else {
        throw std::runtime_error("unsupported held slice type");
      }
      return 0;
    } catch (const std::exception& error) {
      std::cerr << "FAIL: " << error.what() << "\n";
      return 1;
    }
  }
  if (argc != 2) {
    std::cerr << "usage: test_real_gguf_slices <directory> | <Q4_K|Q6_K> <raw-hex> <reference-hex>\n";
    return 2;
  }
  try {
    const std::filesystem::path directory(argv[1]);
    for (const std::string position : {"first", "last"}) {
      compare_slice<prisminfer::kernels::GgmlQ4KBlock>(
          directory, "q4_k-" + position,
          prisminfer::kernels::decode_ggml_q4_k_reference);
      compare_slice<prisminfer::kernels::GgmlQ6KBlock>(
          directory, "q6_k-" + position,
          prisminfer::kernels::decode_ggml_q6_k_reference);
      const auto f32_raw = read_bytes(directory / ("f32-" + position + ".bin"));
      const auto f32_reference = read_bytes(directory / ("f32-" + position + "-f32.bin"));
      if (f32_raw != f32_reference) {
        throw std::runtime_error("F32 upstream-fallback differential mismatch: " + position);
      }
    }
    std::cout << "Real GGUF F32/Q4_K/Q6_K retained-slice differential PASS\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << "\n";
    return 1;
  }
}
