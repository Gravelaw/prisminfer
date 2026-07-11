#include <iostream>

#include "prisminfer/benchmark_comparator.h"

namespace {
int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}
}  // namespace

int main() {
  prisminfer::BenchmarkCell baseline;
  baseline.model_hash = "model";
  baseline.quantization_format = "Q4_K_M";
  baseline.quant_artifact_sha256 = "quant";
  baseline.context_tokens = 2048;
  baseline.batch_size = 1;
  baseline.prompt_fixture_hash = "fixture";
  baseline.backend = "llama.cpp";
  baseline.os = "windows";
  baseline.gpu_name = "gpu";
  baseline.driver_mode = "wddm";
  baseline.cuda_driver_version = 13030;
  baseline.cuda_runtime_version = 13030;
  baseline.vram_tier_gib = 8;
  baseline.hard_cap_bytes = 8589934592ULL;
  baseline.op_type = "matmul";
  baseline.sequence_phase = "decode";
  baseline.kernel_backend = "ggml-cuda-mmq";
  baseline.kernel_name = "baseline";
  baseline.kernel_version = "1";

  auto candidate = baseline;
  if (expect(prisminfer::compare_benchmark_cells(baseline, candidate).same_cell,
             "identical cells compare")) {
    return 1;
  }

  candidate.kernel_backend = "prisminfer-cuda";
  candidate.kernel_name = "q4-decode-gemv";
  candidate.kernel_version = "2";
  if (expect(prisminfer::compare_benchmark_cells(baseline, candidate).same_cell,
             "kernel variant fields do not change validation cell")) {
    return 1;
  }

  candidate = baseline;
  candidate.batch_size = 2;
  candidate.sequence_phase = "prefill";
  candidate.hard_cap_bytes = 17179869184ULL;
  const auto mismatch =
      prisminfer::compare_benchmark_cells(baseline, candidate);
  if (expect(!mismatch.same_cell, "mismatched cells rejected")) return 1;
  const auto reasons =
      prisminfer::format_mismatch_reasons(mismatch.mismatch_reasons);
  if (expect(reasons.find("batch_size_mismatch") != std::string::npos,
             "batch mismatch reported")) {
    return 1;
  }
  if (expect(reasons.find("sequence_phase_mismatch") != std::string::npos,
             "sequence mismatch reported")) {
    return 1;
  }
  if (expect(reasons.find("hard_cap_bytes_mismatch") != std::string::npos,
             "hard cap mismatch reported")) {
    return 1;
  }
  if (expect(reasons.find("kernel_backend_mismatch") == std::string::npos,
             "kernel backend variant not reported as identity mismatch")) {
    return 1;
  }
  return 0;
}
