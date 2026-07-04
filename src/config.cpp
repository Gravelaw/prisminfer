#include "prisminfer/config.h"

#include <charconv>
#include <chrono>
#include <sstream>

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
      config.hard_cap_bytes = value;
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
  --telemetry prisminfer-probe.jsonl
  --manifest prisminfer-manifest.json
  --max-model-bytes 1099511627776
  --max-sidecar-bytes 65536
)";
}

}  // namespace prisminfer
