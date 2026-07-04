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

enum class BackendKind {
  Null,
  Fake,
  Llama,
};

struct RuntimeConfig {
  RunMode mode{RunMode::OneGbSafeCpu};
  std::uint64_t hard_cap_bytes{kOneGiB};
  std::string model_parameter_bucket;
  std::uint64_t parameter_count{0};
  std::uint32_t vram_tier_gib{1};
  std::string validation_cell_id;
  std::string validation_cell_status{"not-started"};
  BackendKind backend{BackendKind::Null};
  bool backend_required{false};
  std::filesystem::path dependency_pin_file;
  std::filesystem::path llama_executable_path;
  std::uint64_t context_tokens{512};
  std::uint32_t gpu_layers{0};
  bool mmap_enabled{true};
  std::uint64_t warmup_tokens{1};
  bool kv_accounting{false};
  std::uint32_t kv_layer_count{0};
  std::uint32_t kv_head_count{0};
  std::uint32_t kv_head_dim{0};
  std::uint32_t kv_block_tokens{16};
  std::string kv_placement{"backend"};
  std::string kv_compression{"none"};
  std::uint32_t kv_key_bits{16};
  std::uint32_t kv_value_bits{16};
  std::uint64_t kv_metadata_budget_bytes{0};
  std::string quality_gate{"none"};
  std::filesystem::path quality_baseline_manifest;
  double quality_baseline_score{1.0};
  double quality_observed_score{1.0};
  double quality_max_delta{0.0};
  bool quality_retrieval_passed{true};
  bool quality_deterministic_match{true};
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
std::string to_string(BackendKind backend);
std::optional<BackendKind> parse_backend_kind(const std::string& value);
std::string make_run_id();
ParseResult parse_args(const std::vector<std::string>& args);
std::string usage_text();

}  // namespace prisminfer
