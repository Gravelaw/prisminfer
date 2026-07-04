#include <iostream>
#include <string>
#include <vector>

#include "prisminfer/config.h"

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
  {
    const auto parsed = prisminfer::parse_args({});
    if (expect(parsed.config.has_value(), "default args parse")) return 1;
    if (expect(parsed.config->mode == prisminfer::RunMode::OneGbSafeCpu,
               "default mode is CPU safe")) return 1;
    if (expect(parsed.config->hard_cap_bytes == prisminfer::kOneGiB,
               "default hard cap is one GiB")) return 1;
  }
  {
    const std::vector<std::string> args = {
        "--mode", "1gb-safe-gpu-probed", "--hard-cap-bytes", "2048",
        "--telemetry", "out.jsonl", "--manifest", "manifest.json",
        "--run-id", "run-123", "--model", "model.gguf", "--sidecar",
        "model.gguf.prism.json", "--max-model-bytes", "8192",
        "--max-sidecar-bytes", "512", "--simulate-allocator-peak-bytes", "4096",
        "--simulate-process-gpu-peak-bytes", "4096",
        "--simulate-warmup-peak-bytes", "1024",
        "--simulate-unknown-post-warmup-bytes", "1"};
    const auto parsed = prisminfer::parse_args(args);
    if (expect(parsed.config.has_value(), "explicit args parse")) return 1;
    if (expect(parsed.config->mode == prisminfer::RunMode::OneGbSafeGpuProbed,
               "explicit GPU-probed mode")) return 1;
    if (expect(parsed.config->hard_cap_bytes == 2048,
               "explicit hard cap")) return 1;
    if (expect(parsed.config->telemetry_path == "out.jsonl",
               "explicit telemetry path")) return 1;
    if (expect(parsed.config->manifest_path == "manifest.json",
               "explicit manifest path")) return 1;
    if (expect(parsed.config->run_id == "run-123", "explicit run id")) {
      return 1;
    }
    if (expect(parsed.config->model_path == "model.gguf",
               "explicit model path")) return 1;
    if (expect(parsed.config->sidecar_path == "model.gguf.prism.json",
               "explicit sidecar path")) return 1;
    if (expect(parsed.config->max_model_bytes == 8192,
               "explicit model size limit")) return 1;
    if (expect(parsed.config->max_sidecar_bytes == 512,
               "explicit sidecar size limit")) return 1;
    if (expect(parsed.config->simulate_allocator_peak_bytes == 4096,
               "explicit allocator simulation")) return 1;
    if (expect(parsed.config->simulate_process_gpu_peak_bytes == 4096,
               "explicit process simulation")) return 1;
    if (expect(parsed.config->simulate_warmup_peak_bytes == 1024,
               "explicit warmup simulation")) return 1;
    if (expect(parsed.config->simulate_unknown_post_warmup_bytes == 1,
               "explicit unknown post-warmup simulation")) return 1;
  }
  {
    const auto parsed = prisminfer::parse_args({"--mode", "invalid"});
    if (expect(!parsed.config.has_value(), "invalid mode rejected")) return 1;
    if (expect(parsed.error.find("unsupported mode") != std::string::npos,
               "invalid mode error is specific")) return 1;
  }
  return 0;
}
