#include "prisminfer/benchmark_comparator.h"

#include <sstream>

namespace prisminfer {

namespace {

template <typename T>
void compare_field(const char* name,
                   const T& baseline,
                   const T& candidate,
                   std::vector<std::string>* reasons) {
  if (baseline != candidate) {
    reasons->emplace_back(std::string{name} + "_mismatch");
  }
}

}  // namespace

BenchmarkComparison compare_benchmark_cells(const BenchmarkCell& baseline,
                                            const BenchmarkCell& candidate) {
  BenchmarkComparison result;
  compare_field("model_hash", baseline.model_hash, candidate.model_hash,
                &result.mismatch_reasons);
  compare_field("quantization_format", baseline.quantization_format,
                candidate.quantization_format, &result.mismatch_reasons);
  compare_field("quant_artifact_sha256", baseline.quant_artifact_sha256,
                candidate.quant_artifact_sha256, &result.mismatch_reasons);
  compare_field("context_tokens", baseline.context_tokens,
                candidate.context_tokens, &result.mismatch_reasons);
  compare_field("batch_size", baseline.batch_size, candidate.batch_size,
                &result.mismatch_reasons);
  compare_field("prompt_fixture_hash", baseline.prompt_fixture_hash,
                candidate.prompt_fixture_hash, &result.mismatch_reasons);
  compare_field("backend", baseline.backend, candidate.backend,
                &result.mismatch_reasons);
  compare_field("os", baseline.os, candidate.os, &result.mismatch_reasons);
  compare_field("gpu_name", baseline.gpu_name, candidate.gpu_name,
                &result.mismatch_reasons);
  compare_field("driver_mode", baseline.driver_mode, candidate.driver_mode,
                &result.mismatch_reasons);
  compare_field("cuda_driver_version", baseline.cuda_driver_version,
                candidate.cuda_driver_version, &result.mismatch_reasons);
  compare_field("cuda_runtime_version", baseline.cuda_runtime_version,
                candidate.cuda_runtime_version, &result.mismatch_reasons);
  compare_field("vram_tier_gib", baseline.vram_tier_gib,
                candidate.vram_tier_gib, &result.mismatch_reasons);
  compare_field("hard_cap_bytes", baseline.hard_cap_bytes,
                candidate.hard_cap_bytes, &result.mismatch_reasons);
  compare_field("op_type", baseline.op_type, candidate.op_type,
                &result.mismatch_reasons);
  compare_field("sequence_phase", baseline.sequence_phase,
                candidate.sequence_phase, &result.mismatch_reasons);
  result.same_cell = result.mismatch_reasons.empty();
  return result;
}

std::string format_mismatch_reasons(
    const std::vector<std::string>& mismatch_reasons) {
  std::ostringstream out;
  for (std::size_t i = 0; i < mismatch_reasons.size(); ++i) {
    if (i != 0) {
      out << ",";
    }
    out << mismatch_reasons[i];
  }
  return out.str();
}

}  // namespace prisminfer
