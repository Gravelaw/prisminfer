#include "prisminfer/backend.h"

namespace prisminfer {
namespace {

class NullBackendAdapter final : public BackendAdapter {
 public:
  std::string name() const override { return "null"; }
  std::string version() const override { return kBackendContractVersion; }

  BackendWarmupResult warmup(const RuntimeConfig&,
                             CappedAllocatorTracker& allocator) override {
    BackendWarmupResult result;
    result.memory_sample = allocator.sample();
    return result;
  }
};

}  // namespace

std::unique_ptr<BackendAdapter> make_fake_backend_adapter();
std::unique_ptr<BackendAdapter> make_llama_backend_adapter();

BackendPlan make_backend_plan(const RuntimeConfig& config) {
  BackendPlan plan;
  plan.backend_name = to_string(config.backend);
  plan.backend_version = kBackendContractVersion;
  plan.model_path = config.model_path;
  plan.model_parameter_bucket = config.model_parameter_bucket;
  plan.parameter_count = config.parameter_count;
  plan.requested_context_tokens = config.context_tokens;
  plan.hard_vram_cap_bytes = config.hard_cap_bytes;
  plan.vram_tier_gib = config.vram_tier_gib;
  plan.gpu_layers = config.gpu_layers;
  plan.mmap_enabled = config.mmap_enabled;
  plan.gpu_requested = gpu_requested(config.mode);
  return plan;
}

std::unique_ptr<BackendAdapter> make_backend_adapter(BackendKind backend) {
  if (backend == BackendKind::Null) {
    return std::make_unique<NullBackendAdapter>();
  }
  if (backend == BackendKind::Fake) {
    return make_fake_backend_adapter();
  }
  if (backend == BackendKind::Llama) {
#if defined(PRISMINFER_ENABLE_LLAMA_BACKEND)
    return make_llama_backend_adapter();
#else
    return nullptr;
#endif
  }
  return nullptr;
}

}  // namespace prisminfer
