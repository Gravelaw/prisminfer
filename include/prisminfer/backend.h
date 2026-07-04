#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "prisminfer/allocator_tracker.h"
#include "prisminfer/config.h"
#include "prisminfer/memory_cap.h"

namespace prisminfer {

inline constexpr const char* kBackendContractVersion = "0.2";

struct BackendPlan {
  std::string backend_name;
  std::string backend_version;
  std::filesystem::path model_path;
  std::string model_parameter_bucket;
  std::uint64_t parameter_count{0};
  std::uint64_t model_size_bytes{0};
  std::uint64_t requested_context_tokens{0};
  std::uint64_t hard_vram_cap_bytes{0};
  std::uint32_t vram_tier_gib{0};
  std::uint32_t gpu_layers{0};
  bool mmap_enabled{true};
  bool gpu_requested{false};
};

struct BackendWarmupResult {
  bool ok{true};
  std::string failure_reason;
  MemorySample memory_sample;
  std::uint64_t backend_owned_peak_bytes{0};
  std::uint64_t backend_external_peak_bytes{0};
  std::uint64_t retained_pool_bytes{0};
};

class BackendAdapter {
 public:
  virtual ~BackendAdapter() = default;
  virtual std::string name() const = 0;
  virtual std::string version() const = 0;
  virtual BackendWarmupResult warmup(const RuntimeConfig& config,
                                     CappedAllocatorTracker& allocator) = 0;
};

BackendPlan make_backend_plan(const RuntimeConfig& config);
std::unique_ptr<BackendAdapter> make_backend_adapter(BackendKind backend);

}  // namespace prisminfer
