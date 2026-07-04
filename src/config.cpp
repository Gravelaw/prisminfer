#include "prisminfer/config.h"

#include <charconv>
#include <chrono>
#include <sstream>

#include "prisminfer/gpu_cap_policy.h"

namespace prisminfer {

std::string to_string(RunMode mode) {
  switch (mode) {
    case RunMode::OneGbSafeCpu:
      return "1gb-safe-cpu";
    case RunMode::OneGbSafeGpuProbed:
      return "1gb-safe-gpu-probed";
  }
  return "unknown";
}

std::optional<RunMode> parse_run_mode(const std::string& value) {
  if (value == "1gb-safe-cpu") {
    return RunMode::OneGbSafeCpu;
  }
  if (value == "1gb-safe-gpu-probed") {
    return RunMode::OneGbSafeGpuProbed;
  }
  return std::nullopt;
}

bool gpu_requested(RunMode mode) {
  return mode == RunMode::OneGbSafeGpuProbed;
}

std::string to_string(BackendKind backend) {
  switch (backend) {
    case BackendKind::Null:
      return "null";
    case BackendKind::Fake:
      return "fake";
    case BackendKind::Llama:
      return "llama";
  }
  return "unknown";
}

std::optional<BackendKind> parse_backend_kind(const std::string& value) {
  if (value == "null") {
    return BackendKind::Null;
  }
  if (value == "fake") {
    return BackendKind::Fake;
  }
  if (value == "llama") {
    return BackendKind::Llama;
  }
  return std::nullopt;
}

std::string make_run_id() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  const auto ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
  std::ostringstream out;
  out << "prisminfer-" << ns;
  return out.str();
}

namespace {

bool parse_u64(const std::string& value, std::uint64_t* output) {
  const char* begin = value.data();
  const char* end = value.data() + value.size();
  std::uint64_t parsed = 0;
  const auto result = std::from_chars(begin, end, parsed);
  if (result.ec != std::errc{} || result.ptr != end) {
    return false;
  }
  *output = parsed;
  return true;
}

bool parse_u32(const std::string& value, std::uint32_t* output) {
  std::uint64_t parsed = 0;
  if (!parse_u64(value, &parsed) || parsed > UINT32_MAX) {
    return false;
  }
  *output = static_cast<std::uint32_t>(parsed);
  return true;
}

bool valid_validation_cell_status(const std::string& value) {
  return value == "not-started" || value == "metadata-only" ||
         value == "warmup" || value == "decode-smoke" ||
         value == "quality-gated" || value == "profitable" ||
         value == "validated" ||
         value == "rejected";
}

bool parse_on_off(const std::string& value, bool* output) {
  if (value == "on") {
    *output = true;
    return true;
  }
  if (value == "off") {
    *output = false;
    return true;
  }
  return false;
}

bool parse_bool_text(const std::string& value, bool* output) {
  if (value == "true") {
    *output = true;
    return true;
  }
  if (value == "false") {
    *output = false;
    return true;
  }
  return false;
}

bool valid_kv_placement(const std::string& value) {
  return value == "backend" || value == "gpu" || value == "host" ||
         value == "mixed";
}

bool valid_kv_compression(const std::string& value) {
  return value == "none" || value == "accounting-only" ||
         value == "reference" || value == "experimental";
}

bool valid_quality_gate_name(const std::string& value) {
  return value == "none" || value == "smoke" || value == "retrieval" ||
         value == "long-context";
}

bool valid_profitability_policy_name(const std::string& value) {
  return value == "off" || value == "warn" || value == "fail-closed";
}

bool valid_offload_policy_name(const std::string& value) {
  return value == "none" || value == "gpu" || value == "host-kv" ||
         value == "nvme-simulated" || value == "nvme-experimental";
}

bool parse_double_value(const std::string& value, double* output) {
  std::istringstream in(value);
  double parsed = 0.0;
  in >> parsed;
  if (!in || !in.eof()) {
    return false;
  }
  *output = parsed;
  return true;
}

}  // namespace

ParseResult parse_args(const std::vector<std::string>& args) {
  RuntimeConfig config;
  config.run_id = make_run_id();

  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "--help" || arg == "-h") {
      config.show_help = true;
      return ParseResult{config, ""};
    }
    if (arg == "--mode") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--mode requires a value"};
      }
      const auto mode = parse_run_mode(args[++i]);
      if (!mode.has_value()) {
        return ParseResult{std::nullopt, "unsupported mode: " + args[i]};
      }
      config.mode = *mode;
      continue;
    }
    if (arg == "--hard-cap-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--hard-cap-bytes requires a value"};
      }
      std::uint64_t value = 0;
      if (!parse_u64(args[++i], &value) || value == 0) {
        return ParseResult{std::nullopt,
                           "--hard-cap-bytes must be a positive integer"};
      }
      const auto cap_policy = validate_gpu_hard_cap(value);
      if (!cap_policy.accepted) {
        return ParseResult{std::nullopt, cap_policy.failure_reason};
      }
      config.hard_cap_bytes = value;
      continue;
    }
    if (arg == "--model-parameter-bucket") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--model-parameter-bucket requires a value"};
      }
      config.model_parameter_bucket = args[++i];
      continue;
    }
    if (arg == "--parameter-count") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--parameter-count requires a value"};
      }
      if (!parse_u64(args[++i], &config.parameter_count)) {
        return ParseResult{std::nullopt,
                           "--parameter-count must be an integer"};
      }
      continue;
    }
    if (arg == "--vram-tier-gib") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--vram-tier-gib requires a value"};
      }
      if (!parse_u32(args[++i], &config.vram_tier_gib) ||
          config.vram_tier_gib == 0) {
        return ParseResult{std::nullopt,
                           "--vram-tier-gib must be a positive integer"};
      }
      const std::uint64_t tier_bytes =
          static_cast<std::uint64_t>(config.vram_tier_gib) * kOneGiB;
      if (!validate_gpu_hard_cap(tier_bytes).accepted) {
        return ParseResult{std::nullopt, "vram_tier_exceeds_max_gpu_cap"};
      }
      continue;
    }
    if (arg == "--validation-cell-id") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--validation-cell-id requires a value"};
      }
      config.validation_cell_id = args[++i];
      continue;
    }
    if (arg == "--validation-cell-status") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--validation-cell-status requires a value"};
      }
      config.validation_cell_status = args[++i];
      if (!valid_validation_cell_status(config.validation_cell_status)) {
        return ParseResult{std::nullopt,
                           "unsupported validation cell status: " +
                               config.validation_cell_status};
      }
      continue;
    }
    if (arg == "--backend") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--backend requires a value"};
      }
      const auto backend = parse_backend_kind(args[++i]);
      if (!backend.has_value()) {
        return ParseResult{std::nullopt, "unsupported backend: " + args[i]};
      }
      config.backend = *backend;
      continue;
    }
    if (arg == "--backend-required") {
      config.backend_required = true;
      continue;
    }
    if (arg == "--dependency-pin-file") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--dependency-pin-file requires a path"};
      }
      config.dependency_pin_file = args[++i];
      continue;
    }
    if (arg == "--llama-executable") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--llama-executable requires a path"};
      }
      config.llama_executable_path = args[++i];
      continue;
    }
    if (arg == "--context-tokens") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--context-tokens requires a value"};
      }
      if (!parse_u64(args[++i], &config.context_tokens) ||
          config.context_tokens == 0) {
        return ParseResult{std::nullopt,
                           "--context-tokens must be a positive integer"};
      }
      continue;
    }
    if (arg == "--gpu-layers") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--gpu-layers requires a value"};
      }
      if (!parse_u32(args[++i], &config.gpu_layers)) {
        return ParseResult{std::nullopt, "--gpu-layers must be an integer"};
      }
      continue;
    }
    if (arg == "--mmap") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--mmap requires on or off"};
      }
      if (!parse_on_off(args[++i], &config.mmap_enabled)) {
        return ParseResult{std::nullopt, "--mmap must be on or off"};
      }
      continue;
    }
    if (arg == "--warmup-tokens") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--warmup-tokens requires a value"};
      }
      if (!parse_u64(args[++i], &config.warmup_tokens) ||
          config.warmup_tokens == 0) {
        return ParseResult{std::nullopt,
                           "--warmup-tokens must be a positive integer"};
      }
      continue;
    }
    if (arg == "--kv-accounting") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--kv-accounting requires on or off"};
      }
      if (!parse_on_off(args[++i], &config.kv_accounting)) {
        return ParseResult{std::nullopt, "--kv-accounting must be on or off"};
      }
      continue;
    }
    if (arg == "--kv-layer-count") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--kv-layer-count requires a value"};
      }
      if (!parse_u32(args[++i], &config.kv_layer_count)) {
        return ParseResult{std::nullopt,
                           "--kv-layer-count must be an integer"};
      }
      continue;
    }
    if (arg == "--kv-head-count") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--kv-head-count requires a value"};
      }
      if (!parse_u32(args[++i], &config.kv_head_count)) {
        return ParseResult{std::nullopt, "--kv-head-count must be an integer"};
      }
      continue;
    }
    if (arg == "--kv-head-dim") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--kv-head-dim requires a value"};
      }
      if (!parse_u32(args[++i], &config.kv_head_dim)) {
        return ParseResult{std::nullopt, "--kv-head-dim must be an integer"};
      }
      continue;
    }
    if (arg == "--kv-block-tokens") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--kv-block-tokens requires a value"};
      }
      if (!parse_u32(args[++i], &config.kv_block_tokens) ||
          config.kv_block_tokens == 0) {
        return ParseResult{std::nullopt,
                           "--kv-block-tokens must be a positive integer"};
      }
      continue;
    }
    if (arg == "--kv-placement") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--kv-placement requires a value"};
      }
      config.kv_placement = args[++i];
      if (!valid_kv_placement(config.kv_placement)) {
        return ParseResult{std::nullopt,
                           "unsupported kv placement: " +
                               config.kv_placement};
      }
      continue;
    }
    if (arg == "--kv-compression") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--kv-compression requires a value"};
      }
      config.kv_compression = args[++i];
      if (!valid_kv_compression(config.kv_compression)) {
        return ParseResult{std::nullopt,
                           "unsupported kv compression: " +
                               config.kv_compression};
      }
      continue;
    }
    if (arg == "--kv-key-bits") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--kv-key-bits requires a value"};
      }
      if (!parse_u32(args[++i], &config.kv_key_bits) ||
          config.kv_key_bits == 0) {
        return ParseResult{std::nullopt,
                           "--kv-key-bits must be a positive integer"};
      }
      continue;
    }
    if (arg == "--kv-value-bits") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--kv-value-bits requires a value"};
      }
      if (!parse_u32(args[++i], &config.kv_value_bits) ||
          config.kv_value_bits == 0) {
        return ParseResult{std::nullopt,
                           "--kv-value-bits must be a positive integer"};
      }
      continue;
    }
    if (arg == "--kv-metadata-budget-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--kv-metadata-budget-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.kv_metadata_budget_bytes)) {
        return ParseResult{
            std::nullopt,
            "--kv-metadata-budget-bytes must be an integer"};
      }
      continue;
    }
    if (arg == "--quality-gate") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--quality-gate requires a value"};
      }
      config.quality_gate = args[++i];
      if (!valid_quality_gate_name(config.quality_gate)) {
        return ParseResult{std::nullopt,
                           "unsupported quality gate: " +
                               config.quality_gate};
      }
      continue;
    }
    if (arg == "--quality-baseline-manifest") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--quality-baseline-manifest requires a path"};
      }
      config.quality_baseline_manifest = args[++i];
      continue;
    }
    if (arg == "--quality-baseline-score") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--quality-baseline-score requires a value"};
      }
      if (!parse_double_value(args[++i], &config.quality_baseline_score)) {
        return ParseResult{std::nullopt,
                           "--quality-baseline-score must be a number"};
      }
      continue;
    }
    if (arg == "--quality-observed-score") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--quality-observed-score requires a value"};
      }
      if (!parse_double_value(args[++i], &config.quality_observed_score)) {
        return ParseResult{std::nullopt,
                           "--quality-observed-score must be a number"};
      }
      continue;
    }
    if (arg == "--quality-max-delta") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--quality-max-delta requires a value"};
      }
      if (!parse_double_value(args[++i], &config.quality_max_delta) ||
          config.quality_max_delta < 0.0) {
        return ParseResult{std::nullopt,
                           "--quality-max-delta must be a non-negative number"};
      }
      continue;
    }
    if (arg == "--quality-retrieval-passed") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--quality-retrieval-passed requires true or false"};
      }
      if (!parse_bool_text(args[++i], &config.quality_retrieval_passed)) {
        return ParseResult{std::nullopt,
                           "--quality-retrieval-passed must be true or false"};
      }
      continue;
    }
    if (arg == "--quality-deterministic-match") {
      if (i + 1 >= args.size()) {
        return ParseResult{
            std::nullopt,
            "--quality-deterministic-match requires true or false"};
      }
      if (!parse_bool_text(args[++i], &config.quality_deterministic_match)) {
        return ParseResult{
            std::nullopt,
            "--quality-deterministic-match must be true or false"};
      }
      continue;
    }
    if (arg == "--profitability-policy") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--profitability-policy requires a value"};
      }
      config.profitability_policy = args[++i];
      if (!valid_profitability_policy_name(config.profitability_policy)) {
        return ParseResult{std::nullopt,
                           "unsupported profitability policy: " +
                               config.profitability_policy};
      }
      continue;
    }
    if (arg == "--baseline-manifest") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--baseline-manifest requires a path"};
      }
      config.baseline_manifest = args[++i];
      continue;
    }
    if (arg == "--min-speedup-ratio") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--min-speedup-ratio requires a value"};
      }
      if (!parse_double_value(args[++i], &config.min_speedup_ratio) ||
          config.min_speedup_ratio <= 0.0) {
        return ParseResult{std::nullopt,
                           "--min-speedup-ratio must be a positive number"};
      }
      continue;
    }
    if (arg == "--offload-policy") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--offload-policy requires a value"};
      }
      config.offload_policy = args[++i];
      if (!valid_offload_policy_name(config.offload_policy)) {
        return ParseResult{std::nullopt,
                           "unsupported offload policy: " +
                               config.offload_policy};
      }
      continue;
    }
    if (arg == "--pinned-host-budget-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--pinned-host-budget-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.pinned_host_budget_bytes)) {
        return ParseResult{
            std::nullopt,
            "--pinned-host-budget-bytes must be an integer"};
      }
      continue;
    }
    if (arg == "--staging-buffer-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--staging-buffer-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.staging_buffer_bytes)) {
        return ParseResult{std::nullopt,
                           "--staging-buffer-bytes must be an integer"};
      }
      continue;
    }
    if (arg == "--transfer-metrics") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--transfer-metrics requires on or off"};
      }
      if (!parse_on_off(args[++i], &config.transfer_metrics)) {
        return ParseResult{std::nullopt,
                           "--transfer-metrics must be on or off"};
      }
      continue;
    }
    if (arg == "--cold-cache-run") {
      config.cold_cache_run = true;
      continue;
    }
    if (arg == "--cpu-baseline-ttft-ms") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--cpu-baseline-ttft-ms requires a value"};
      }
      if (!parse_double_value(args[++i], &config.cpu_baseline_ttft_ms)) {
        return ParseResult{std::nullopt,
                           "--cpu-baseline-ttft-ms must be a number"};
      }
      continue;
    }
    if (arg == "--cpu-baseline-decode-tps") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--cpu-baseline-decode-tps requires a value"};
      }
      if (!parse_double_value(args[++i], &config.cpu_baseline_decode_tps)) {
        return ParseResult{std::nullopt,
                           "--cpu-baseline-decode-tps must be a number"};
      }
      continue;
    }
    if (arg == "--cpu-baseline-peak-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--cpu-baseline-peak-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.cpu_baseline_peak_bytes)) {
        return ParseResult{std::nullopt,
                           "--cpu-baseline-peak-bytes must be an integer"};
      }
      continue;
    }
    if (arg == "--observed-ttft-ms") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--observed-ttft-ms requires a value"};
      }
      if (!parse_double_value(args[++i], &config.observed_ttft_ms)) {
        return ParseResult{std::nullopt,
                           "--observed-ttft-ms must be a number"};
      }
      continue;
    }
    if (arg == "--observed-decode-tps") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--observed-decode-tps requires a value"};
      }
      if (!parse_double_value(args[++i], &config.observed_decode_tps)) {
        return ParseResult{std::nullopt,
                           "--observed-decode-tps must be a number"};
      }
      continue;
    }
    if (arg == "--token-latency-p50-ms") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--token-latency-p50-ms requires a value"};
      }
      if (!parse_double_value(args[++i], &config.token_latency_p50_ms)) {
        return ParseResult{std::nullopt,
                           "--token-latency-p50-ms must be a number"};
      }
      continue;
    }
    if (arg == "--token-latency-p95-ms") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--token-latency-p95-ms requires a value"};
      }
      if (!parse_double_value(args[++i], &config.token_latency_p95_ms)) {
        return ParseResult{std::nullopt,
                           "--token-latency-p95-ms must be a number"};
      }
      continue;
    }
    if (arg == "--transfer-h2d-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--transfer-h2d-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.transfer_h2d_bytes)) {
        return ParseResult{std::nullopt,
                           "--transfer-h2d-bytes must be an integer"};
      }
      continue;
    }
    if (arg == "--transfer-d2h-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--transfer-d2h-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.transfer_d2h_bytes)) {
        return ParseResult{std::nullopt,
                           "--transfer-d2h-bytes must be an integer"};
      }
      continue;
    }
    if (arg == "--nvme-read-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--nvme-read-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.nvme_read_bytes)) {
        return ParseResult{std::nullopt,
                           "--nvme-read-bytes must be an integer"};
      }
      continue;
    }
    if (arg == "--nvme-write-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--nvme-write-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.nvme_write_bytes)) {
        return ParseResult{std::nullopt,
                           "--nvme-write-bytes must be an integer"};
      }
      continue;
    }
    if (arg == "--transfer-h2d-ms") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--transfer-h2d-ms requires a value"};
      }
      if (!parse_double_value(args[++i], &config.transfer_h2d_ms)) {
        return ParseResult{std::nullopt,
                           "--transfer-h2d-ms must be a number"};
      }
      continue;
    }
    if (arg == "--transfer-d2h-ms") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--transfer-d2h-ms requires a value"};
      }
      if (!parse_double_value(args[++i], &config.transfer_d2h_ms)) {
        return ParseResult{std::nullopt,
                           "--transfer-d2h-ms must be a number"};
      }
      continue;
    }
    if (arg == "--transfer-io-ms") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--transfer-io-ms requires a value"};
      }
      if (!parse_double_value(args[++i], &config.transfer_io_ms)) {
        return ParseResult{std::nullopt, "--transfer-io-ms must be a number"};
      }
      continue;
    }
    if (arg == "--transfer-wait-ms") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--transfer-wait-ms requires a value"};
      }
      if (!parse_double_value(args[++i], &config.transfer_wait_ms)) {
        return ParseResult{std::nullopt,
                           "--transfer-wait-ms must be a number"};
      }
      continue;
    }
    if (arg == "--prefill-ms") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--prefill-ms requires a value"};
      }
      if (!parse_double_value(args[++i], &config.prefill_ms)) {
        return ParseResult{std::nullopt, "--prefill-ms must be a number"};
      }
      continue;
    }
    if (arg == "--decode-ms") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--decode-ms requires a value"};
      }
      if (!parse_double_value(args[++i], &config.decode_ms)) {
        return ParseResult{std::nullopt, "--decode-ms must be a number"};
      }
      continue;
    }
    if (arg == "--telemetry") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--telemetry requires a path"};
      }
      config.telemetry_path = args[++i];
      continue;
    }
    if (arg == "--manifest") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--manifest requires a path"};
      }
      config.manifest_path = args[++i];
      continue;
    }
    if (arg == "--run-id") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--run-id requires a value"};
      }
      config.run_id = args[++i];
      if (config.run_id.empty()) {
        return ParseResult{std::nullopt, "--run-id must not be empty"};
      }
      continue;
    }
    if (arg == "--model") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--model requires a path"};
      }
      config.model_path = args[++i];
      continue;
    }
    if (arg == "--sidecar") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--sidecar requires a path"};
      }
      config.sidecar_path = args[++i];
      continue;
    }
    if (arg == "--max-model-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt, "--max-model-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.max_model_bytes) ||
          config.max_model_bytes == 0) {
        return ParseResult{std::nullopt,
                           "--max-model-bytes must be a positive integer"};
      }
      continue;
    }
    if (arg == "--max-sidecar-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--max-sidecar-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.max_sidecar_bytes) ||
          config.max_sidecar_bytes == 0) {
        return ParseResult{std::nullopt,
                           "--max-sidecar-bytes must be a positive integer"};
      }
      continue;
    }
    if (arg == "--simulate-allocator-peak-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--simulate-allocator-peak-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.simulate_allocator_peak_bytes)) {
        return ParseResult{
            std::nullopt,
            "--simulate-allocator-peak-bytes must be an integer"};
      }
      continue;
    }
    if (arg == "--simulate-process-gpu-peak-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{
            std::nullopt,
            "--simulate-process-gpu-peak-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.simulate_process_gpu_peak_bytes)) {
        return ParseResult{
            std::nullopt,
            "--simulate-process-gpu-peak-bytes must be an integer"};
      }
      continue;
    }
    if (arg == "--simulate-warmup-peak-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{std::nullopt,
                           "--simulate-warmup-peak-bytes requires a value"};
      }
      if (!parse_u64(args[++i], &config.simulate_warmup_peak_bytes)) {
        return ParseResult{std::nullopt,
                           "--simulate-warmup-peak-bytes must be an integer"};
      }
      continue;
    }
    if (arg == "--simulate-unknown-post-warmup-bytes") {
      if (i + 1 >= args.size()) {
        return ParseResult{
            std::nullopt,
            "--simulate-unknown-post-warmup-bytes requires a value"};
      }
      if (!parse_u64(args[++i],
                     &config.simulate_unknown_post_warmup_bytes)) {
        return ParseResult{
            std::nullopt,
            "--simulate-unknown-post-warmup-bytes must be an integer"};
      }
      continue;
    }
    return ParseResult{std::nullopt, "unknown argument: " + arg};
  }

  return ParseResult{config, ""};
}

std::string usage_text() {
  return R"(prism-probe

Usage:
  prism-probe [--mode 1gb-safe-cpu|1gb-safe-gpu-probed]
              [--hard-cap-bytes BYTES]
              [--model-parameter-bucket BUCKET]
              [--parameter-count N]
              [--vram-tier-gib N]
              [--validation-cell-id ID]
              [--validation-cell-status STATUS]
              [--backend null|fake|llama]
              [--backend-required]
              [--dependency-pin-file PATH]
              [--llama-executable PATH]
              [--context-tokens N]
              [--gpu-layers N]
              [--mmap on|off]
              [--warmup-tokens N]
              [--kv-accounting on|off]
              [--kv-layer-count N]
              [--kv-head-count N]
              [--kv-head-dim N]
              [--kv-block-tokens N]
              [--kv-placement backend|gpu|host|mixed]
              [--kv-compression none|accounting-only|reference|experimental]
              [--kv-key-bits N]
              [--kv-value-bits N]
              [--kv-metadata-budget-bytes BYTES]
              [--quality-gate none|smoke|retrieval|long-context]
              [--quality-baseline-manifest PATH]
              [--quality-baseline-score N]
              [--quality-observed-score N]
              [--quality-max-delta N]
              [--quality-retrieval-passed true|false]
              [--quality-deterministic-match true|false]
              [--profitability-policy off|warn|fail-closed]
              [--baseline-manifest PATH]
              [--min-speedup-ratio R]
              [--offload-policy none|gpu|host-kv|nvme-simulated|nvme-experimental]
              [--pinned-host-budget-bytes BYTES]
              [--staging-buffer-bytes BYTES]
              [--transfer-metrics on|off]
              [--cold-cache-run]
              [--cpu-baseline-ttft-ms N]
              [--cpu-baseline-decode-tps N]
              [--cpu-baseline-peak-bytes BYTES]
              [--observed-ttft-ms N]
              [--observed-decode-tps N]
              [--token-latency-p50-ms N]
              [--token-latency-p95-ms N]
              [--transfer-h2d-bytes BYTES]
              [--transfer-d2h-bytes BYTES]
              [--nvme-read-bytes BYTES]
              [--nvme-write-bytes BYTES]
              [--transfer-h2d-ms N]
              [--transfer-d2h-ms N]
              [--transfer-io-ms N]
              [--transfer-wait-ms N]
              [--prefill-ms N]
              [--decode-ms N]
              [--telemetry PATH]
              [--manifest PATH]
              [--run-id ID]
              [--model PATH]
              [--sidecar PATH]
              [--max-model-bytes BYTES]
              [--max-sidecar-bytes BYTES]
              [--simulate-allocator-peak-bytes BYTES]
              [--simulate-process-gpu-peak-bytes BYTES]
              [--simulate-warmup-peak-bytes BYTES]
              [--simulate-unknown-post-warmup-bytes BYTES]

Defaults:
  --mode 1gb-safe-cpu
  --hard-cap-bytes 1073741824
  maximum hard cap bytes 17179869184
  --telemetry prisminfer-probe.jsonl
  --manifest prisminfer-manifest.json
  --max-model-bytes 1099511627776
  --max-sidecar-bytes 65536
)";
}

}  // namespace prisminfer
