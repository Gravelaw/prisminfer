#include <iostream>
#include <string>
#include <vector>

#include "prisminfer/benchmark_comparator.h"
#include "prisminfer/errors.h"
#include "prisminfer/telemetry.h"

namespace {

std::string value_after(const std::vector<std::string>& args,
                        const std::string& flag) {
  for (std::size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == flag) {
      return args[i + 1];
    }
  }
  return "";
}

std::uint32_t u32_after(const std::vector<std::string>& args,
                        const std::string& flag) {
  const auto value = value_after(args, flag);
  if (value.empty()) {
    return 0;
  }
  return static_cast<std::uint32_t>(std::stoul(value));
}

std::uint64_t u64_after(const std::vector<std::string>& args,
                        const std::string& flag) {
  const auto value = value_after(args, flag);
  if (value.empty()) {
    return 0;
  }
  return static_cast<std::uint64_t>(std::stoull(value));
}

prisminfer::BenchmarkCell cell_from_args(const std::vector<std::string>& args,
                                         const std::string& prefix) {
  prisminfer::BenchmarkCell cell;
  cell.model_hash = value_after(args, prefix + "-model-hash");
  cell.quantization_format = value_after(args, prefix + "-quantization-format");
  cell.quant_artifact_sha256 =
      value_after(args, prefix + "-quant-artifact-sha256");
  cell.context_tokens = u64_after(args, prefix + "-context-tokens");
  cell.batch_size = u32_after(args, prefix + "-batch-size");
  cell.prompt_fixture_hash = value_after(args, prefix + "-prompt-fixture-hash");
  cell.backend = value_after(args, prefix + "-backend");
  cell.os = value_after(args, prefix + "-os");
  cell.gpu_name = value_after(args, prefix + "-gpu-name");
  cell.driver_mode = value_after(args, prefix + "-driver-mode");
  cell.cuda_driver_version = u32_after(args, prefix + "-cuda-driver-version");
  cell.cuda_runtime_version = u32_after(args, prefix + "-cuda-runtime-version");
  cell.vram_tier_gib = u32_after(args, prefix + "-vram-tier-gib");
  cell.op_type = value_after(args, prefix + "-op-type");
  cell.sequence_phase = value_after(args, prefix + "-sequence-phase");
  cell.kernel_backend = value_after(args, prefix + "-kernel-backend");
  cell.kernel_name = value_after(args, prefix + "-kernel-name");
  cell.kernel_version = value_after(args, prefix + "-kernel-version");
  return cell;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  const auto baseline = cell_from_args(args, "--baseline");
  const auto candidate = cell_from_args(args, "--candidate");
  const auto comparison =
      prisminfer::compare_benchmark_cells(baseline, candidate);
  std::cout << "{\"same_cell\":"
            << (comparison.same_cell ? "true" : "false")
            << ",\"mismatch_reasons\":\""
            << prisminfer::json_escape(
                   prisminfer::format_mismatch_reasons(
                       comparison.mismatch_reasons))
            << "\"}\n";
  return comparison.same_cell
             ? static_cast<int>(prisminfer::ExitCode::Ok)
             : static_cast<int>(prisminfer::ExitCode::FailedClosed);
}
