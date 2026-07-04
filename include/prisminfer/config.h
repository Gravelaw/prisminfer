#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace prisminfer {

constexpr std::uint64_t kOneGiB = 1'073'741'824ULL;

enum class RunMode {
  OneGbSafeCpu,
  OneGbSafeGpuProbed,
};

struct RuntimeConfig {
  RunMode mode{RunMode::OneGbSafeCpu};
  std::uint64_t hard_cap_bytes{kOneGiB};
  std::string telemetry_path{"prisminfer-probe.jsonl"};
  std::string manifest_path{"prisminfer-manifest.json"};
  std::string run_id;
  std::filesystem::path model_path;
  std::filesystem::path sidecar_path;
  std::uint64_t max_model_bytes{1ULL << 40};
  std::uint64_t max_sidecar_bytes{64ULL * 1024ULL};
  std::uint64_t simulate_allocator_peak_bytes{0};
  std::uint64_t simulate_process_gpu_peak_bytes{0};
  std::uint64_t simulate_warmup_peak_bytes{0};
  std::uint64_t simulate_unknown_post_warmup_bytes{0};
  bool show_help{false};
};

struct ParseResult {
  std::optional<RuntimeConfig> config;
  std::string error;
};

std::string to_string(RunMode mode);
std::optional<RunMode> parse_run_mode(const std::string& value);
bool gpu_requested(RunMode mode);
std::string make_run_id();
ParseResult parse_args(const std::vector<std::string>& args);
std::string usage_text();

}  // namespace prisminfer
