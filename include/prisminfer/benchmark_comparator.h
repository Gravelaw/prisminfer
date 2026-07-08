#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace prisminfer {

struct BenchmarkCell {
  std::string model_hash;
  std::string quantization_format;
  std::string quant_artifact_sha256;
  std::uint64_t context_tokens{0};
  std::uint32_t batch_size{0};
  std::string prompt_fixture_hash;
  std::string backend;
  std::string os;
  std::string gpu_name;
  std::string driver_mode;
  std::uint32_t cuda_driver_version{0};
  std::uint32_t cuda_runtime_version{0};
  std::uint32_t vram_tier_gib{0};
  std::uint64_t hard_cap_bytes{0};
  std::string op_type;
  std::string sequence_phase;
  std::string kernel_backend;
  std::string kernel_name;
  std::string kernel_version;
};

struct BenchmarkComparison {
  bool same_cell{false};
  std::vector<std::string> mismatch_reasons;
};

BenchmarkComparison compare_benchmark_cells(const BenchmarkCell& baseline,
                                            const BenchmarkCell& candidate);

std::string format_mismatch_reasons(
    const std::vector<std::string>& mismatch_reasons);

}  // namespace prisminfer
