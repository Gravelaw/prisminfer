#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace prisminfer {

constexpr std::uint64_t kDefaultMaxModelBytes = 1ULL << 40;
constexpr std::uint64_t kDefaultMaxSidecarBytes = 64ULL * 1024ULL;

struct ModelSidecarValidationRequest {
  std::filesystem::path model_path;
  std::filesystem::path sidecar_path;
  std::uint64_t max_model_bytes{kDefaultMaxModelBytes};
  std::uint64_t max_sidecar_bytes{kDefaultMaxSidecarBytes};
};

struct ModelSidecarValidationResult {
  bool skipped{true};
  bool valid{true};
  std::string failure_reason;
  std::filesystem::path normalized_model_path;
  std::filesystem::path normalized_sidecar_path;
  std::uint64_t model_size_bytes{0};
  std::uint64_t sidecar_size_bytes{0};
  std::string model_sha256;
};

ModelSidecarValidationResult validate_model_sidecar(
    const ModelSidecarValidationRequest& request);

}  // namespace prisminfer
