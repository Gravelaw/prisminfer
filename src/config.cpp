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
              [--context-tokens N]
              [--gpu-layers N]
              [--mmap on|off]
              [--warmup-tokens N]
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
