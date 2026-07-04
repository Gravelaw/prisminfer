#include "prisminfer/backend.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

namespace prisminfer {
namespace {

std::string quote_text(const std::string& text) {
  std::string escaped;
  escaped.reserve(text.size() + 2);
  escaped.push_back('"');
  for (const char ch : text) {
    if (ch == '"') {
      escaped.push_back('\\');
    }
    escaped.push_back(ch);
  }
  escaped.push_back('"');
  return escaped;
}

std::string quote_path(const std::filesystem::path& value) {
  return quote_text(value.string());
}

bool path_has_directory(const std::filesystem::path& path) {
  return path.has_parent_path() || path.is_absolute();
}

std::filesystem::path resolve_llama_executable(const RuntimeConfig& config) {
  if (!config.llama_executable_path.empty()) {
    return config.llama_executable_path;
  }
#if defined(_WIN32)
  char* env_path = nullptr;
  std::size_t env_size = 0;
  if (_dupenv_s(&env_path, &env_size, "PRISMINFER_LLAMA_CLI") == 0 &&
      env_path != nullptr && env_path[0] != '\0') {
    std::filesystem::path resolved(env_path);
    std::free(env_path);
    return resolved;
  }
  if (env_path != nullptr) {
    std::free(env_path);
  }
#else
  const char* env_path = std::getenv("PRISMINFER_LLAMA_CLI");
  if (env_path != nullptr && env_path[0] != '\0') {
    return std::filesystem::path(env_path);
  }
#endif
  return {};
}

std::uint64_t parse_bytes(double value, const std::string& unit) {
  double multiplier = 1.0;
  if (unit == "KiB") {
    multiplier = 1024.0;
  } else if (unit == "MiB") {
    multiplier = 1024.0 * 1024.0;
  } else if (unit == "GiB") {
    multiplier = 1024.0 * 1024.0 * 1024.0;
  }
  if (value <= 0.0) {
    return 0;
  }
  return static_cast<std::uint64_t>(value * multiplier);
}

std::uint64_t extract_llama_allocation_bytes(
    const std::filesystem::path& log_path) {
  std::ifstream in(log_path);
  if (!in) {
    return 0;
  }

  std::uint64_t total = 0;
  std::string line;
  const std::regex size_pattern(
      R"(([0-9]+(?:\.[0-9]+)?)\s*(B|KiB|MiB|GiB))");
  while (std::getline(in, line)) {
    const bool allocation_line =
        line.find("buffer size") != std::string::npos ||
        line.find("llama_kv_cache: size =") != std::string::npos;
    if (!allocation_line) {
      continue;
    }
    auto begin = std::sregex_iterator(line.begin(), line.end(), size_pattern);
    const auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
      const double value = std::stod((*it)[1].str());
      total += parse_bytes(value, (*it)[2].str());
    }
  }
  return total;
}

class LlamaBackendAdapter final : public BackendAdapter {
 public:
  std::string name() const override { return "llama"; }
  std::string version() const override { return kBackendContractVersion; }

  BackendWarmupResult warmup(const RuntimeConfig& config,
                             CappedAllocatorTracker& allocator) override {
    BackendWarmupResult result;
    result.memory_sample = allocator.sample();
    const auto executable = resolve_llama_executable(config);
    if (executable.empty()) {
      result.ok = false;
      result.failure_reason = "llama_executable_required";
      return result;
    }
    if (config.model_path.empty()) {
      result.ok = false;
      result.failure_reason = "llama_model_required";
      return result;
    }
    if (!std::filesystem::exists(config.model_path)) {
      result.ok = false;
      result.failure_reason = "llama_model_not_found";
      return result;
    }
    if (path_has_directory(executable) && !std::filesystem::exists(executable)) {
      result.ok = false;
      result.failure_reason = "llama_executable_not_found";
      return result;
    }

    const auto log_path =
        std::filesystem::temp_directory_path() /
        ("prisminfer-llama-" + config.run_id + ".log");
    std::ostringstream command;
#if defined(_WIN32)
    command << "call ";
#endif
    command << quote_path(executable) << " --model " << quote_path(config.model_path)
            << " --ctx-size " << config.context_tokens << " --n-predict "
            << config.warmup_tokens << " --prompt "
            << quote_text("PrismInfer backend warmup.") << " --no-display-prompt"
            << " --no-conversation --single-turn --log-verbose"
            << " --gpu-layers " << config.gpu_layers;
    if (!config.mmap_enabled) {
      command << " --no-mmap";
    }
    command << " > " << quote_path(log_path) << " 2>&1";

    const int exit_code = std::system(command.str().c_str());
    if (exit_code != 0) {
      result.ok = false;
      result.failure_reason = "llama_cli_failed";
      return result;
    }

    result.backend_external_peak_bytes =
        extract_llama_allocation_bytes(log_path);
    if (result.backend_external_peak_bytes == 0) {
      result.ok = false;
      result.failure_reason = "llama_allocation_evidence_missing";
      return result;
    }
    return result;
  }
};

}  // namespace

std::unique_ptr<BackendAdapter> make_llama_backend_adapter() {
  return std::make_unique<LlamaBackendAdapter>();
}

}  // namespace prisminfer
